#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace arcade::hal {

/// Thin wrapper over the Linux calls the codec set-up makes (SR-HAL-10): the sysfs GPIO files
/// for HPS_I2C_CONTROL and the I2C character device for the WM8731. Injectable so unit tests
/// replace it with a fake, as BridgeSys does for the bridge (#34). No call throws (R-4.1.5).
/// Every failure reports its `errno` value.
class CodecSys {
  public:
    CodecSys() = default;
    CodecSys(const CodecSys&) = delete;
    CodecSys& operator=(const CodecSys&) = delete;
    CodecSys(CodecSys&&) = delete;
    CodecSys& operator=(CodecSys&&) = delete;
    virtual ~CodecSys() = default;

    /// The entry names in directory `path` (without "." and ".."). Returns 0 or errno.
    virtual int listDir(const std::string& path, std::vector<std::string>& names) noexcept = 0;

    /// Read the small text file `path` (a sysfs attribute) into `text`. Returns 0 or errno.
    virtual int readTextFile(const std::string& path, std::string& text) noexcept = 0;

    /// Write `text` to the file `path` (a sysfs attribute). Returns 0 or errno.
    virtual int writeTextFile(const std::string& path, const std::string& text) noexcept = 0;

    /// open(path, O_RDWR). Returns a file descriptor, or -1 with errno stored in `err`.
    virtual int openDevice(const std::string& path, int& err) noexcept = 0;

    /// ioctl(fd, I2C_SLAVE, address): the 7-bit address later writes go to. Returns 0 or errno.
    virtual int setI2cAddress(int fd, std::uint16_t address) noexcept = 0;

    /// write(fd, data, length) as one I2C message. Returns 0 if every byte was sent and
    /// acknowledged, otherwise errno (a short write counts as EIO).
    virtual int writeBytes(int fd, const std::uint8_t* data, std::size_t length) noexcept = 0;

    /// close(2).
    virtual void close(int fd) noexcept = 0;
};

/// The real implementation, calling the Linux kernel (used on the board).
class LinuxCodecSys final : public CodecSys {
  public:
    LinuxCodecSys() = default;
    int listDir(const std::string& path, std::vector<std::string>& names) noexcept override;
    int readTextFile(const std::string& path, std::string& text) noexcept override;
    int writeTextFile(const std::string& path, const std::string& text) noexcept override;
    int openDevice(const std::string& path, int& err) noexcept override;
    int setI2cAddress(int fd, std::uint16_t address) noexcept override;
    int writeBytes(int fd, const std::uint8_t* data, std::size_t length) noexcept override;
    void close(int fd) noexcept override;
};

}  // namespace arcade::hal
