// SPDX-License-Identifier: Apache-2.0
#include "hik_bridge/bridge.hpp"

#include <chrono>
#include <fcntl.h>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace hik_bridge;
using namespace std::chrono_literals;

#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(\
    "check failed at line " + std::to_string(__LINE__)); } while (false)

template <class F> void fails(F action, std::string_view message) {
    try { action(); }
    catch (const std::exception& e) { CHECK(std::string_view(e.what()).find(message) != std::string_view::npos); return; }
    throw std::runtime_error("expected failure");
}

template <class F> void eventually(F condition) {
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (!condition()) {
        CHECK(std::chrono::steady_clock::now() < end);
        std::this_thread::sleep_for(1ms);
    }
}

struct Fake final : Backend {
    std::string fail;
    std::uint32_t ver = 1;
    std::vector<std::string> calls;
    Events* events = nullptr;
    bool step(std::string name) { calls.push_back(name); return name != fail; }
    std::uint32_t version() const noexcept override { return ver; }
    bool initialize() override { return step("initialize"); }
    bool login() override { return step("login"); }
    bool arm(Events& e) override { events = &e; return step("arm"); }
    bool disarm() noexcept override {
        const char late[] = "late-synthetic";
        events->alarm(0x70000002, late, sizeof(late));
        return step("disarm");
    }
    bool logout() noexcept override { return step("logout"); }
    bool cleanup() noexcept override { return step("cleanup"); }
};

void lifecycle() {
    for (const auto& stage : {"version", "initialize", "login", "arm"}) {
        Fake fake;
        fake.fail = stage;
        if (fake.fail == "version") fake.ver = 2;
        Events events(8, [](auto) {});
        fails([&] { Session session(fake, events, 1); },
            fake.fail == "version" ? "version" : fake.fail == "arm" ? "arming" :
            fake.fail == "initialize" ? "initialization" : fake.fail);
        if (fake.fail == "version") CHECK(fake.calls.empty());
        if (fake.fail == "initialize") CHECK(fake.calls == std::vector<std::string>{"initialize"});
        if (fake.fail == "login") CHECK((fake.calls == std::vector<std::string>{"initialize", "login", "cleanup"}));
        if (fake.fail == "arm") CHECK((fake.calls == std::vector<std::string>{"initialize", "login", "arm", "logout", "cleanup"}));
    }
    for (const auto& stage : {"", "disarm", "logout", "cleanup"}) {
        Fake fake;
        fake.fail = stage;
        Events events(8, [](auto) {});
        {
            Session session(fake, events, 1);
            CHECK(session.stop() == fake.fail.empty());
            CHECK(session.stop() == fake.fail.empty());
        }
        CHECK((fake.calls == std::vector<std::string>{"initialize", "login", "arm", "disarm", "logout", "cleanup"}));
        CHECK(events.counters().accepted == events.counters().delivered);
        CHECK(events.counters().rejected == 0);
        const char byte = 0;
        CHECK(!events.alarm(1, &byte, 1));
        CHECK(events.counters().rejected == 1);
    }
}

void queue_limits() {
    fails([] { Events e(0, [](auto) {}); }, "configuration");
    fails([] { Events e(4097, [](auto) {}); }, "configuration");
    fails([] { Events e(1, {}); }, "configuration");
    std::promise<void> entered, release;
    auto gate = release.get_future().share();
    std::vector<Metadata> output;
    Events events(1, [&](Metadata event) {
        output.push_back(event);
        if (output.size() == 1) { entered.set_value(); gate.wait(); }
    });
    const char synthetic[] = "synthetic-sensitive-canary";
    eventually([&] { return events.alarm(0xfedcba98, synthetic, sizeof(synthetic)); });
    // Ensure a failed assertion cannot leave the worker blocked during unwinding.
    struct Release { std::promise<void>& promise; ~Release() { promise.set_value(); } };
    {
        Release unblock{release};
        CHECK(entered.get_future().wait_for(2s) == std::future_status::ready);
        CHECK(events.alarm(2, synthetic, sizeof(synthetic)));
        CHECK(!events.alarm(3, synthetic, sizeof(synthetic)));
        CHECK(!events.alarm(4, nullptr, 1));
        CHECK(!events.alarm(4, synthetic, 0));
        CHECK(!events.alarm(4, synthetic, Events::max_payload_length + 1));
        CHECK(!events.exception(0x70000003));
        CHECK(events.connection_fault());
    }
    events.stop();
    CHECK(output.size() == 2);
    CHECK(output[0].code == 0xfedcba98);
    CHECK(output[0].received_unix_ms > 0);
    const auto stats = events.counters();
    CHECK(stats.accepted == 2 && stats.delivered == 2 && stats.dropped >= 2 && stats.invalid == 3);
    const auto json = format_metadata("camera-demo", output[0], stats);
    CHECK(json.find(synthetic) == std::string::npos);
    CHECK(json.find("received_unix_ms") != std::string::npos);
    fails([&] { format_metadata("bad\"alias", output[0], stats); }, "alias");

    Events broken(4, [](auto) { throw std::runtime_error("synthetic-secret"); });
    eventually([&] { return broken.alarm(1, synthetic, 1); });
    broken.stop();
    CHECK(broken.counters().output_errors == 1 && broken.counters().delivered == 0);
}

void concurrent_shutdown() {
    Events events(4, [](auto) { std::this_thread::sleep_for(10us); });
    std::atomic<std::uint64_t> attempts{0};
    std::vector<std::jthread> producers;
    for (int i = 0; i < 4; ++i) {
        producers.emplace_back([&](std::stop_token stop) {
            const char byte = 0;
            while (!stop.stop_requested()) { ++attempts; events.alarm(7, &byte, 1); }
        });
    }
    eventually([&] { return events.counters().accepted > 10; });
    events.stop();
    for (auto& thread : producers) thread.request_stop();
    producers.clear();
    const auto stats = events.counters();
    CHECK(stats.accepted == stats.delivered);
    CHECK(attempts == stats.accepted + stats.dropped + stats.rejected);
    CHECK(stats.dropped > 0);
    Events exceptions(4, [](auto) {});
    eventually([&] { return exceptions.exception(99); });
    exceptions.stop();
    CHECK(exceptions.connection_fault());
    CHECK(exceptions.counters().delivered == 1);
}

void passwords() {
    char pattern[] = "/tmp/hik-bridge-synthetic-XXXXXX";
    const int fd = mkstemp(pattern);
    CHECK(fd >= 0);
    close(fd);
    const std::string path = pattern;
    const std::string linked = path + "-link";
    struct Remove { std::string path, linked; ~Remove() { unlink(linked.c_str()); unlink(path.c_str()); } } remove{path, linked};
    auto write = [&](const std::string& value) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << value;
        CHECK(bool(file));
    };
    write("synthetic-password\r\n");
    CHECK(Password(path).value() == "synthetic-password");
    chmod(path.c_str(), 0644);
    fails([&] { Password p(path); }, "unsafe");
    chmod(path.c_str(), 0600);
    CHECK(symlink(path.c_str(), linked.c_str()) == 0);
    fails([&] { Password p(linked); }, "open");
    unlink(linked.c_str());
    CHECK(link(path.c_str(), linked.c_str()) == 0);
    fails([&] { Password p(path); }, "unsafe");
    unlink(linked.c_str());
    for (const auto& bad : {std::string{}, std::string("\n"), std::string("a\nb"),
                           std::string("a\0b", 3), std::string(128, 'x')}) {
        write(bad);
        fails([&] { Password p(path); }, "password");
    }
    CHECK(mkfifo(linked.c_str(), 0600) == 0);
    fails([&] { Password p(linked); }, "unsafe");
}

int main(int argc, char** argv) {
    try {
        CHECK(argc == 3);
        lifecycle();
        queue_limits();
        concurrent_shutdown();
        passwords();
        fails([] { reject_unverified_sdk("relative"); }, "absolute");
        fails([] { reject_unverified_sdk("/nonexistent/synthetic-sdk.so"); }, "unavailable");
        fails([&] { reject_unverified_sdk(argv[1]); }, "missing symbol");
        fails([&] { reject_unverified_sdk(argv[2]); }, "not verified");
        CHECK(valid_alias("camera-demo") && !valid_alias("") && !valid_alias(std::string(49, 'a')));
        std::cout << "synthetic bridge checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
