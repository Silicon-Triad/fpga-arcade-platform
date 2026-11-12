#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace arcade::hal {

/// Thin wrapper over the Linux calls the HAL makes to map the bridge window (SR-HAL-01) and
/// to check readiness (SR-HAL-09), so unit tests can replace them with fakes (as sw/input/
/// does with EvdevSys). No call throws (R-4.1.5). Every failure reports its `errno` value.
class BridgeSys {
  public:
    BridgeSys() = default;
    BridgeSys(const BridgeSys&) = delete;
    BridgeSys& operator=(const BridgeSys&) = delete;
    BridgeSys(BridgeSys&&) = delete;
    BridgeSys& operator=(BridgeSys&&) = delete;
    virtual ~BridgeSys() = default;

    /// Read the small text file `path` (a sysfs attribute) into `text`. Returns 0, or the
    /// `errno` value on failure (then `text` is unspecified).
    virtual int readTextFile(const std::string& path, std::string& text) noexcept = 0;

    /// `open(2)`. Returns a file descriptor, or -1 with the `errno` value stored in `err`.
    virtual int open(const std::string& path, int flags, int& err) noexcept = 0;

    /// `close(2)`.
    virtual void close(int fd) noexcept = 0;

    /// `mmap(nullptr, length, prot, flags, fd, offset)`. Returns the mapping, or nullptr
    /// (for `MAP_FAILED`) with the `errno` value stored in `err`.
    virtual void* mmap(std::size_t length, int prot, int flags, int fd, std::uint64_t offset,
                       int& err) noexcept = 0;

    /// `munmap(2)`.
    virtual void munmap(void* addr, std::size_t length) noexcept = 0;
};

/// The real implementation, calling the Linux kernel (used on the board).
class LinuxBridgeSys final : public BridgeSys {
  public:
    LinuxBridgeSys() = default;
    int readTextFile(const std::string& path, std::string& text) noexcept override;
    int open(const std::string& path, int flags, int& err) noexcept override;
    void close(int fd) noexcept override;
    void* mmap(std::size_t length, int prot, int flags, int fd, std::uint64_t offset,
               int& err) noexcept override;
    void munmap(void* addr, std::size_t length) noexcept override;
};

}  // namespace arcade::hal
