// SPDX-License-Identifier: Apache-2.0
#include "hik_bridge/bridge.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace hik_bridge {

struct Events::Impl {
    std::vector<Metadata> ring;
    Sink sink;
    std::mutex mutex;
    std::condition_variable ready;
    std::size_t head = 0, size = 0;
    bool closed = false;
    std::atomic<bool> fault{false};
    std::atomic<std::uint64_t> accepted{0}, delivered{0}, dropped{0}, invalid{0},
        rejected{0}, output_errors{0};
    std::thread worker;

    Impl(std::size_t capacity, Sink output) : ring(capacity), sink(std::move(output)) {}
    void run() {
        std::unique_lock lock(mutex);
        while (true) {
            ready.wait(lock, [this] { return closed || size != 0; });
            if (size == 0) return;
            const Metadata event = ring[head];
            head = (head + 1) % ring.size();
            --size;
            lock.unlock();
            try {
                sink(event);
                ++delivered;
            } catch (...) {
                ++output_errors;
            }
            lock.lock();
        }
    }
};

Events::Events(std::size_t capacity, Sink sink) {
    if (capacity == 0 || capacity > 4096 || !sink)
        throw std::invalid_argument("invalid queue configuration");
    impl_ = std::make_unique<Impl>(capacity, std::move(sink));
    impl_->worker = std::thread([this] { impl_->run(); });
}

Events::~Events() { stop(); }

bool Events::alarm(std::uint32_t code, const void* payload, std::uint32_t length) noexcept {
    if (!payload || length == 0 || length > max_payload_length) {
        ++impl_->invalid;
        return false;
    }
    // Retain only scalar metadata: SDK payloads may contain borrowed pointers or secrets.
    return enqueue(false, code, length);
}

bool Events::exception(std::uint32_t code) noexcept {
    impl_->fault = true;
    return enqueue(true, code, 0);
}

bool Events::enqueue(bool exception, std::uint32_t code, std::uint32_t length) noexcept {
    std::unique_lock lock(impl_->mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
        ++impl_->dropped;
        return false;
    }
    if (impl_->closed) {
        ++impl_->rejected;
        return false;
    }
    if (impl_->size == impl_->ring.size()) {
        ++impl_->dropped;
        return false;
    }
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    impl_->ring[(impl_->head + impl_->size) % impl_->ring.size()] =
        Metadata{exception, code, length, now};
    ++impl_->size;
    ++impl_->accepted;
    lock.unlock();
    impl_->ready.notify_one();
    return true;
}

void Events::stop() {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->closed = true;
    }
    impl_->ready.notify_one();
    if (impl_->worker.joinable()) impl_->worker.join();
}

Counters Events::counters() const noexcept {
    return {impl_->accepted.load(), impl_->delivered.load(), impl_->dropped.load(),
        impl_->invalid.load(), impl_->rejected.load(), impl_->output_errors.load()};
}
bool Events::connection_fault() const noexcept { return impl_->fault.load(); }

Session::Session(Backend& backend, Events& events, std::uint32_t expected_version)
    : backend_(backend), events_(events) {
    try {
        if (backend_.version() != expected_version)
            throw std::runtime_error("backend version mismatch");
        if (!backend_.initialize()) throw std::runtime_error("backend initialization failed");
        initialized_ = true;
        if (!backend_.login()) throw std::runtime_error("login failed");
        logged_in_ = true;
        if (!backend_.arm(events_)) throw std::runtime_error("arming failed");
        armed_ = true;
    } catch (...) {
        const bool clean = stop();
        if (!clean) throw std::runtime_error("startup and cleanup failed");
        throw;
    }
}

Session::~Session() { stop(); }
bool Session::stop() noexcept {
    if (stopped_) return stop_ok_;
    stopped_ = true;
    if (armed_ && !backend_.disarm()) stop_ok_ = false;
    if (logged_in_ && !backend_.logout()) stop_ok_ = false;
    if (initialized_ && !backend_.cleanup()) stop_ok_ = false;
    events_.stop();
    return stop_ok_;
}

bool valid_alias(std::string_view alias) noexcept {
    if (alias.empty() || alias.size() > 48) return false;
    for (const char c : alias) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    }
    return true;
}

std::string format_metadata(std::string_view alias, const Metadata& event, const Counters& c) {
    if (!valid_alias(alias)) throw std::invalid_argument("invalid device alias");
    return "{\"device\":\"" + std::string(alias) + "\",\"kind\":\"" +
        (event.exception ? "exception" : "alarm") + "\",\"code\":" +
        std::to_string(event.code) + ",\"received_unix_ms\":" +
        std::to_string(event.received_unix_ms) + ",\"length\":" +
        std::to_string(event.length) + ",\"dropped\":" + std::to_string(c.dropped) +
        ",\"invalid\":" + std::to_string(c.invalid) + "}";
}

} // namespace hik_bridge
