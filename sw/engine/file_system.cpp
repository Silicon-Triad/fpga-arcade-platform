#include "engine/file_system.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace arcade {

std::optional<std::string> PosixFileSystem::readAll(const std::string& path) noexcept {
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return std::nullopt;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        if (in.bad()) {
            return std::nullopt;
        }
        return buffer.str();
    } catch (...) {
        return std::nullopt;  // SR-FW-08: an unreadable file means an empty table.
    }
}

int PosixFileSystem::openForWrite(const std::string& path) noexcept {
    constexpr mode_t kFileMode = 0644;  // Owner read-write, others read.
    return ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, kFileMode);
}

bool PosixFileSystem::writeAll(int fd, std::string_view data) noexcept {
    while (!data.empty()) {
        const ssize_t n = ::write(fd, data.data(), data.size());
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(n));
    }
    return true;
}

bool PosixFileSystem::sync(int fd) noexcept { return ::fsync(fd) == 0; }

bool PosixFileSystem::close(int fd) noexcept { return ::close(fd) == 0; }

bool PosixFileSystem::rename(const std::string& from, const std::string& to) noexcept {
    return std::rename(from.c_str(), to.c_str()) == 0;
}

}  // namespace arcade
