/// @file
/// Thin file-system layer used by the high-score store, so tests can inject a fake and check
/// the save order temp file -> fsync() -> rename() (SR-FW-08, SR-FW-09).
#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace arcade {

/// The file operations the high-score store needs (SR-FW-08, SR-FW-09). No call throws.
class FileSystem {
  public:
    virtual ~FileSystem() = default;

    /// Reads a whole file; std::nullopt if it is missing or unreadable (SR-FW-08).
    virtual std::optional<std::string> readAll(const std::string& path) noexcept = 0;
    /// Creates or truncates `path` for writing; returns a handle, or -1 on error (SR-FW-09).
    virtual int openForWrite(const std::string& path) noexcept = 0;
    /// Writes all of `data` to `fd`; false on error (SR-FW-09).
    virtual bool writeAll(int fd, std::string_view data) noexcept = 0;
    /// Flushes `fd` to the storage device with fsync(); false on error (SR-FW-09).
    virtual bool sync(int fd) noexcept = 0;
    /// Closes `fd`; false on error (SR-FW-09).
    virtual bool close(int fd) noexcept = 0;
    /// Atomically replaces `to` with `from` using rename(); false on error (SR-FW-09).
    virtual bool rename(const std::string& from, const std::string& to) noexcept = 0;
};

/// The real file system through POSIX open/write/fsync/close/rename (SR-FW-09).
class PosixFileSystem final : public FileSystem {
  public:
    /// See FileSystem::readAll (SR-FW-08).
    std::optional<std::string> readAll(const std::string& path) noexcept override;
    /// See FileSystem::openForWrite (SR-FW-09).
    int openForWrite(const std::string& path) noexcept override;
    /// See FileSystem::writeAll (SR-FW-09).
    bool writeAll(int fd, std::string_view data) noexcept override;
    /// See FileSystem::sync (SR-FW-09).
    bool sync(int fd) noexcept override;
    /// See FileSystem::close (SR-FW-09).
    bool close(int fd) noexcept override;
    /// See FileSystem::rename (SR-FW-09).
    bool rename(const std::string& from, const std::string& to) noexcept override;
};

}  // namespace arcade
