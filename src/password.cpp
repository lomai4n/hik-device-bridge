// SPDX-License-Identifier: Apache-2.0
#include "hik_bridge/bridge.hpp"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace hik_bridge {
namespace {
void wipe(char* bytes, std::size_t size) noexcept {
    volatile char* p = bytes;
    while (size--) *p++ = 0;
}
}

Password::Password(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        throw std::runtime_error("invalid password file path");
    const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) throw std::runtime_error("cannot open password file");
    try {
        struct stat info{};
        if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_uid != geteuid() ||
            (info.st_mode & 077) != 0 || (info.st_mode & S_IRUSR) == 0 ||
            info.st_nlink != 1 || info.st_size < 1 || info.st_size > 127)
            throw std::runtime_error("unsafe password file");
        while (size_ < sizeof(bytes_)) {
            const auto count = read(fd, bytes_ + size_, sizeof(bytes_) - size_);
            if (count < 0 && errno == EINTR) continue;
            if (count < 0) throw std::runtime_error("cannot read password file");
            if (count == 0) break;
            size_ += static_cast<std::size_t>(count);
        }
        if (size_ > 127) throw std::runtime_error("password file too long");
        if (size_ && bytes_[size_ - 1] == '\n') {
            --size_;
            if (size_ && bytes_[size_ - 1] == '\r') --size_;
        }
        if (size_ == 0) throw std::runtime_error("empty password file");
        for (std::size_t i = 0; i < size_; ++i) {
            if (static_cast<unsigned char>(bytes_[i]) < 32 || bytes_[i] == 127)
                throw std::runtime_error("invalid password file contents");
        }
        close(fd);
    } catch (...) {
        close(fd);
        wipe(bytes_, sizeof(bytes_));
        throw;
    }
}
Password::~Password() { wipe(bytes_, sizeof(bytes_)); }
std::string_view Password::value() const noexcept { return {bytes_, size_}; }
} // namespace hik_bridge
