#pragma once

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "hal/codec_config.hpp"
#include "hal/codec_sys.hpp"

namespace arcade::hal::test {

/// Paths used by the tests (fake: the board values are not needed here).
inline constexpr CodecPaths kTestCodecPaths{"/fake/gpio", "ff709000.gpio", "/fake/i2c-0"};

/// Fake Linux calls for configureCodec() (SR-HAL-10). sysfs files are served from `files`;
/// every call is recorded in `calls`, in order; the bytes of every I2C write in `i2cWrites`.
/// By default it looks like the board with the #39 device tree: one gpiochip483 (29 lines,
/// labelled ff709000.gpio) and a codec that acknowledges every write.
class FakeCodecSys final : public CodecSys {
  public:
    FakeCodecSys() {
        dirs["/fake/gpio"] = {"export", "gpiochip483", "unexport"};
        files["/fake/gpio/gpiochip483/label"] = "ff709000.gpio\n";
        files["/fake/gpio/gpiochip483/base"] = "483\n";
        files["/fake/gpio/gpiochip483/ngpio"] = "29\n";
    }

    int listDir(const std::string& path, std::vector<std::string>& names) noexcept override {
        calls.push_back("list " + path);
        const auto it = dirs.find(path);
        if (it == dirs.end()) {
            return ENOENT;
        }
        names = it->second;
        return 0;
    }
    int readTextFile(const std::string& path, std::string& text) noexcept override {
        calls.push_back("read " + path);
        const auto it = files.find(path);
        if (it == files.end()) {
            return ENOENT;
        }
        text = it->second;
        return 0;
    }
    int writeTextFile(const std::string& path, const std::string& text) noexcept override {
        calls.push_back("write " + path + " " + text);
        const auto it = writeErrors.find(path);
        return (it == writeErrors.end()) ? 0 : it->second;
    }
    int openDevice(const std::string& path, int& err) noexcept override {
        calls.push_back("open " + path);
        err = openError;
        return (openError == 0) ? kFd : -1;
    }
    int setI2cAddress(int fd, std::uint16_t address) noexcept override {
        calls.push_back("addr " + std::to_string(fd) + " " + std::to_string(address));
        return addressError;
    }
    int writeBytes(int fd, const std::uint8_t* data, std::size_t length) noexcept override {
        calls.push_back("i2c " + std::to_string(fd));
        i2cWrites.emplace_back(data, data + length);
        if (nackOnWrite >= 0 && static_cast<int>(i2cWrites.size()) == nackOnWrite + 1) {
            return EREMOTEIO;
        }
        return 0;
    }
    void close(int fd) noexcept override { calls.push_back("close " + std::to_string(fd)); }

    static constexpr int kFd = 7;
    std::map<std::string, std::vector<std::string>> dirs;
    std::map<std::string, std::string> files;
    std::map<std::string, int> writeErrors;  ///< errno to return for a sysfs write to a path
    int openError = 0;
    int addressError = 0;
    int nackOnWrite = -1;  ///< 0-based index of the I2C write that gets no ACK; -1 = none
    std::vector<std::string> calls;
    std::vector<std::vector<std::uint8_t>> i2cWrites;
};

}  // namespace arcade::hal::test
