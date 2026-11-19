// Real Linux calls behind CodecSys (SR-HAL-10): sysfs GPIO and the I2C character device.
#include <dirent.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstring>

#include "hal/codec_sys.hpp"

namespace arcade::hal {
namespace {

/// Bytes read from one sysfs attribute. Not an SRS value: the attributes read here hold one
/// short word (a chip label or a number), so 64 bytes is ample; longer text is cut off and then
/// fails the comparison, which is the safe side.
constexpr std::size_t kSysfsReadBytes = 64;

}  // namespace

int LinuxCodecSys::listDir(const std::string& path, std::vector<std::string>& names) noexcept {
    DIR* dir = ::opendir(path.c_str());
    if (dir == nullptr) {
        return errno;
    }
    names.clear();
    errno = 0;
    // readdir() is not thread-safe, but only this start-up step reads this directory.
    while (const dirent* entry = ::readdir(dir)) {  // NOLINT(concurrency-mt-unsafe)
        const std::string name(static_cast<const char*>(entry->d_name));
        if (name != "." && name != "..") {
            names.push_back(name);
        }
    }
    const int err = errno;
    static_cast<void>(::closedir(dir));
    return err;
}

int LinuxCodecSys::readTextFile(const std::string& path, std::string& text) noexcept {
    const int fd = ::open(path.c_str(), O_RDONLY);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd < 0) {
        return errno;
    }
    std::array<char, kSysfsReadBytes> buf{};
    const ssize_t n = ::read(fd, buf.data(), buf.size());
    const int readErr = (n < 0) ? errno : 0;
    static_cast<void>(::close(fd));
    if (n < 0) {
        return readErr;
    }
    text.assign(buf.data(), static_cast<std::size_t>(n));
    return 0;
}

int LinuxCodecSys::writeTextFile(const std::string& path, const std::string& text) noexcept {
    const int fd = ::open(path.c_str(), O_WRONLY);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd < 0) {
        return errno;
    }
    // sysfs takes the whole value in one write(); its error (e.g. EBUSY on a second export)
    // comes back from this call.
    const ssize_t n = ::write(fd, text.data(), text.size());
    int err = 0;
    if (n < 0) {
        err = errno;
    } else if (static_cast<std::size_t>(n) != text.size()) {
        err = EIO;
    }
    static_cast<void>(::close(fd));
    return err;
}

int LinuxCodecSys::openDevice(const std::string& path, int& err) noexcept {
    const int fd = ::open(path.c_str(), O_RDWR);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    err = (fd < 0) ? errno : 0;
    return fd;
}

int LinuxCodecSys::setI2cAddress(int fd, std::uint16_t address) noexcept {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): ioctl is variadic.
    if (::ioctl(fd, I2C_SLAVE, static_cast<unsigned long>(address)) < 0) {
        return errno;
    }
    return 0;
}

int LinuxCodecSys::writeBytes(int fd, const std::uint8_t* data, std::size_t length) noexcept {
    // i2c-dev sends one write() as one I2C message; a missing ACK fails the whole call
    // (measured on the board, 19 Nov 2026: "Write failed" after 4 ms, issue #43).
    const ssize_t n = ::write(fd, data, length);
    if (n < 0) {
        return errno;
    }
    return (static_cast<std::size_t>(n) == length) ? 0 : EIO;
}

void LinuxCodecSys::close(int fd) noexcept { static_cast<void>(::close(fd)); }

}  // namespace arcade::hal
