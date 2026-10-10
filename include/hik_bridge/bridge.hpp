// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace hik_bridge {

struct Metadata {
    bool exception;
    std::uint32_t code;
    std::uint32_t length;
    std::int64_t received_unix_ms;
};

struct Counters {
    std::uint64_t accepted, delivered, dropped, invalid, rejected, output_errors;
};

// One worker; concurrent producers. Stop and destruction belong to the owning thread.
class Events {
public:
    static constexpr std::uint32_t max_payload_length = 1024 * 1024;
    using Sink = std::function<void(const Metadata&)>;
    explicit Events(std::size_t capacity, Sink sink);
    ~Events();
    Events(const Events&) = delete;
    Events& operator=(const Events&) = delete;
    bool alarm(std::uint32_t code, const void* payload, std::uint32_t length) noexcept;
    bool exception(std::uint32_t code) noexcept;
    void stop();
    Counters counters() const noexcept;
    bool connection_fault() const noexcept;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool enqueue(bool exception, std::uint32_t code, std::uint32_t length) noexcept;
};

// Generic lifecycle contract, not a vendor ABI. False must leave no partial resource.
class Backend {
public:
    virtual ~Backend() = default;
    virtual std::uint32_t version() const noexcept = 0;
    virtual bool initialize() = 0;
    virtual bool login() = 0;
    virtual bool arm(Events&) = 0;
    virtual bool disarm() noexcept = 0;
    virtual bool logout() noexcept = 0;
    // Must quiesce all callbacks even if cleanup reports failure.
    virtual bool cleanup() noexcept = 0;
};

// Backend and Events must outlive this object; lifecycle calls use one owning thread.
class Session {
public:
    Session(Backend& backend, Events& events, std::uint32_t expected_version);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool stop() noexcept;
private:
    Backend& backend_;
    Events& events_;
    bool initialized_ = false, logged_in_ = false, armed_ = false;
    bool stopped_ = false, stop_ok_ = true;
};

bool valid_alias(std::string_view alias) noexcept;
std::string format_metadata(std::string_view alias, const Metadata&, const Counters&);

// No vendor functions are invoked. Always throws until an ABI/version is verified.
void reject_unverified_sdk(const std::string& absolute_library_path);

class Password {
public:
    explicit Password(const std::string& path);
    ~Password();
    Password(const Password&) = delete;
    Password& operator=(const Password&) = delete;
    std::string_view value() const noexcept;
private:
    char bytes_[128]{};
    std::size_t size_ = 0;
};

} // namespace hik_bridge
