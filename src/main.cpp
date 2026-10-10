// SPDX-License-Identifier: Apache-2.0
#include "hik_bridge/bridge.hpp"

#include <charconv>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void on_signal(int) { interrupted = 1; }

class NonblockingOutput {
public:
    NonblockingOutput() : flags_(fcntl(STDOUT_FILENO, F_GETFL)) {
        if (flags_ < 0 || fcntl(STDOUT_FILENO, F_SETFL, flags_ | O_NONBLOCK) < 0)
            throw std::runtime_error("cannot configure metadata output");
    }
    ~NonblockingOutput() { fcntl(STDOUT_FILENO, F_SETFL, flags_); }
private:
    int flags_;
};

class Simulator final : public hik_bridge::Backend {
public:
    std::uint32_t version() const noexcept override { return 1; }
    bool initialize() override { return true; }
    bool login() override { return true; }
    bool arm(hik_bridge::Events& events) override {
        producer_ = std::jthread([&events](std::stop_token stop) {
            const char synthetic[] = "synthetic-event";
            while (!stop.stop_requested()) {
                events.alarm(0x70000001, synthetic, sizeof(synthetic) - 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
        return true;
    }
    bool disarm() noexcept override {
        producer_.request_stop();
        if (producer_.joinable()) producer_.join();
        return true;
    }
    bool logout() noexcept override { return true; }
    bool cleanup() noexcept override { return disarm(); }
private:
    std::jthread producer_;
};

int seconds(std::string_view input) {
    int value = 0;
    const auto result = std::from_chars(input.data(), input.data() + input.size(), value);
    if (result.ec != std::errc{} || result.ptr != input.data() + input.size() ||
        value < 1 || value > 3600) throw std::invalid_argument("seconds must be 1..3600");
    return value;
}

int simulate(int argc, char** argv) {
    int duration = 1;
    std::string alias = "camera-demo";
    bool got_seconds = false, got_alias = false;
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) throw std::invalid_argument("option value missing");
        const std::string_view option = argv[i];
        if (option == "--seconds" && !got_seconds) {
            duration = seconds(argv[i + 1]);
            got_seconds = true;
        } else if (option == "--alias" && !got_alias) {
            alias = argv[i + 1];
            got_alias = true;
        } else throw std::invalid_argument("unknown or duplicate option");
    }
    if (!hik_bridge::valid_alias(alias)) throw std::invalid_argument("invalid device alias");
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);
    NonblockingOutput output;
    std::cerr << "SIMULATION: synthetic backend; no device connection\n";
    Simulator backend;
    hik_bridge::Events* queue = nullptr;
    hik_bridge::Events events(64, [&](const hik_bridge::Metadata& event) {
        const auto line = hik_bridge::format_metadata(alias, event, queue->counters());
        std::cout << "{\"mode\":\"simulation\"," << line.substr(1) << '\n' << std::flush;
        if (!std::cout) throw std::runtime_error("metadata output failed");
    });
    queue = &events;
    hik_bridge::Session session(backend, events, 1);
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(duration);
    while (!interrupted && !events.connection_fault() &&
           events.counters().output_errors == 0 && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool clean = session.stop();
    const auto c = events.counters();
    std::cerr << "{\"mode\":\"simulation\",\"accepted\":" << c.accepted
        << ",\"delivered\":" << c.delivered << ",\"dropped\":" << c.dropped
        << ",\"invalid\":" << c.invalid << ",\"rejected\":" << c.rejected
        << ",\"output_errors\":" << c.output_errors << "}\n";
    return clean && !events.connection_fault() && c.output_errors == 0 && std::cerr ? 0 : 1;
}
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << "Usage:\n"
                "  hik-device-bridge simulate [--seconds 1..3600] [--alias camera-demo]\n"
                "  hik-device-bridge check-password FILE\n"
                "  hik-device-bridge real --sdk /absolute/path/to/library\n"
                "Real mode is disabled until a Linux SDK ABI/version is verified.\n";
            return std::cout ? 0 : 1;
        }
        if (argc >= 2 && std::string_view(argv[1]) == "simulate") return simulate(argc, argv);
        if (argc == 3 && std::string_view(argv[1]) == "check-password") {
            hik_bridge::Password password(argv[2]);
            std::cout << "password file valid\n";
            return std::cout ? 0 : 1;
        }
        if (argc == 4 && std::string_view(argv[1]) == "real" &&
            std::string_view(argv[2]) == "--sdk") hik_bridge::reject_unverified_sdk(argv[3]);
        throw std::invalid_argument("invalid arguments; use --help");
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
