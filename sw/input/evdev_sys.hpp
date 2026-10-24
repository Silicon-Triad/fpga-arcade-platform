#pragma once

#include <linux/input.h>
#include <sys/types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arcade {

/// Key-capability bitmask returned by `EVIOCGBIT(EV_KEY, ...)`: bit `k` set means the device
/// can report key code `k` (SR-INP-01).
using KeyBits = std::array<std::uint8_t, (KEY_CNT + 7) / 8>;

/// Thin wrapper over the Linux calls the Input Manager makes on `/dev/input`, so that unit tests
/// can replace them with recorded devices and events (SR-INP-01 to SR-INP-04).
class EvdevSys {
  public:
    EvdevSys() = default;
    EvdevSys(const EvdevSys&) = delete;
    EvdevSys& operator=(const EvdevSys&) = delete;
    EvdevSys(EvdevSys&&) = delete;
    EvdevSys& operator=(EvdevSys&&) = delete;
    virtual ~EvdevSys() = default;

    /// Names of the entries in directory `dir`, in no particular order. Empty on error.
    virtual std::vector<std::string> listDir(const std::string& dir) = 0;

    /// `open(2)`. Returns a file descriptor, or -1 on failure.
    virtual int open(const std::string& path, int flags) = 0;

    /// `close(2)`.
    virtual void close(int fd) = 0;

    /// `ioctl(fd, EVIOCGBIT(EV_KEY, ...), bits)`: the keys the device can report (SR-INP-01).
    /// Returns false on failure.
    virtual bool keyBits(int fd, KeyBits& bits) = 0;

    /// `ioctl(fd, EVIOCGRAB, 1)`: exclusive access to the device (SR-INP-03).
    /// Returns false on failure.
    virtual bool grab(int fd) = 0;

    /// `read(2)` of up to `len` bytes into `buf`. Returns the byte count, or -1 with the
    /// `errno` value stored in `err` (for example `EAGAIN` when the queue is empty, SR-INP-04,
    /// or `ENODEV` once the keyboard has been removed, SR-INP-12).
    virtual ssize_t read(int fd, void* buf, std::size_t len, int& err) = 0;

    /// Starts watching directory `dir` with a non-blocking inotify instance for entries being
    /// created (`IN_CREATE`) or having their attributes changed (`IN_ATTRIB`) (SR-INP-15).
    /// Returns the watch file descriptor, or -1 on failure. Close it with close().
    virtual int watchDir(const std::string& dir) = 0;

    /// Reads every queued inotify event from `watchFd` without blocking. Returns true if at
    /// least one event arrived since the last call (SR-INP-15).
    virtual bool dirChanged(int watchFd) = 0;
};

/// The real implementation, calling the Linux kernel.
class LinuxEvdevSys final : public EvdevSys {
  public:
    LinuxEvdevSys() = default;
    std::vector<std::string> listDir(const std::string& dir) override;
    int open(const std::string& path, int flags) override;
    void close(int fd) override;
    bool keyBits(int fd, KeyBits& bits) override;
    bool grab(int fd) override;
    ssize_t read(int fd, void* buf, std::size_t len, int& err) override;
    int watchDir(const std::string& dir) override;
    bool dirChanged(int watchFd) override;
};

}  // namespace arcade
