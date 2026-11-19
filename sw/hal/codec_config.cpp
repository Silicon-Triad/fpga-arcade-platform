#include "hal/codec_config.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "hal/register_map.hpp"

namespace arcade::hal {
namespace {

/// Prefix of the gpiochip entries in the sysfs GPIO class directory (kernel ABI).
constexpr std::string_view kGpioChipPrefix = "gpiochip";
/// Mask of the 9 data bits of one codec write (§3.1.2.4: 9 bits of data).
constexpr unsigned kCodecDataMask = 0x1FF;
/// Mask of the 7-bit register address (§3.1.2.4).
constexpr unsigned kCodecRegMask = 0x7F;
/// Mask of data bits 7..0, sent in the second byte (§3.1.2.4).
constexpr unsigned kCodecLowByteMask = 0xFF;
/// Data bit 8, sent in bit 0 of the first byte (§3.1.2.4).
constexpr unsigned kCodecDataBit8 = 8;

std::string trimRight(std::string s) {
    while (!s.empty() &&
           (s.back() == '\n' || s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

/// Parses a whole non-negative decimal number from sysfs text; false on anything else.
bool parseNonNegative(const std::string& text, long& value) {
    const std::string t = trimRight(text);
    if (t.empty() || t[0] == '-' || t[0] == '+') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    value = std::strtol(t.c_str(), &end, 10);
    return errno == 0 && end != nullptr && *end == '\0';
}

std::string join(std::string_view dir, const std::string& name) {
    return std::string(dir) + "/" + name;
}

/// Step 1: the Linux GPIO number of HPS_I2C_CONTROL, or -1 if no matching chip is usable.
int findHpsI2cControl(CodecSys& sys, const CodecPaths& paths) {
    std::vector<std::string> names;
    if (sys.listDir(std::string(paths.gpioClassDir), names) != 0) {
        return -1;
    }
    for (const std::string& name : names) {
        if (name.compare(0, kGpioChipPrefix.size(), kGpioChipPrefix) != 0) {
            continue;
        }
        const std::string chip = join(paths.gpioClassDir, name);
        std::string label;
        if (sys.readTextFile(chip + "/label", label) != 0 ||
            trimRight(label) != paths.gpioChipLabel) {
            continue;
        }
        std::string baseText;
        std::string ngpioText;
        long base = 0;
        long ngpio = 0;
        if (sys.readTextFile(chip + "/base", baseText) != 0 ||
            sys.readTextFile(chip + "/ngpio", ngpioText) != 0 ||
            !parseNonNegative(baseText, base) || !parseNonNegative(ngpioText, ngpio) ||
            ngpio <= static_cast<long>(kHpsI2cControlBit)) {
            return -1;
        }
        return static_cast<int>(base) + static_cast<int>(kHpsI2cControlBit);
    }
    return -1;
}

/// Step 4: drive HPS_I2C_CONTROL low and unexport it. Best effort: a failure here cannot undo
/// the configuration, and the codec keeps its registers either way.
void releaseHpsI2cControl(CodecSys& sys, const CodecPaths& paths, const std::string& gpioDir,
                          const std::string& number) {
    static_cast<void>(sys.writeTextFile(gpioDir + "/value", "0"));
    static_cast<void>(sys.writeTextFile(join(paths.gpioClassDir, "unexport"), number));
}

}  // namespace

std::array<std::uint8_t, 2> encodeCodecWrite(const CodecWrite& write) noexcept {
    const unsigned data = static_cast<unsigned>(write.data) & kCodecDataMask;
    const unsigned reg = static_cast<unsigned>(write.reg) & kCodecRegMask;
    const auto first = static_cast<std::uint8_t>((reg << 1U) | ((data >> kCodecDataBit8) & 1U));
    const auto second = static_cast<std::uint8_t>(data & kCodecLowByteMask);
    return {first, second};
}

CodecResult configureCodec(CodecSys& sys, RegisterSpace& regs, const CodecPaths& paths) noexcept {
    CodecResult result;

    // (1) SR-HAL-10, §3.1.2.4: HPS_I2C_CONTROL = line kHpsI2cControlBit of the GPIO1 chip.
    const int gpio = findHpsI2cControl(sys, paths);
    if (gpio < 0) {
        result.status = CodecStatus::kGpioChipNotFound;
        return result;
    }
    result.gpio = gpio;
    const std::string number = std::to_string(gpio);
    const std::string gpioDir = join(paths.gpioClassDir, "gpio" + number);

    // (2) Hand the codec's I2C bus to the HPS (§3.1.2.4). EBUSY: already exported, which is fine.
    int err = sys.writeTextFile(join(paths.gpioClassDir, "export"), number);
    if (err != 0 && err != EBUSY) {
        result.status = CodecStatus::kGpioSetupFailed;
        result.sysError = err;
        return result;
    }
    err = sys.writeTextFile(gpioDir + "/direction", "out");
    if (err == 0) {
        err = sys.writeTextFile(gpioDir + "/value", "1");
    }
    if (err != 0) {
        releaseHpsI2cControl(sys, paths, gpioDir, number);
        result.status = CodecStatus::kGpioSetupFailed;
        result.sysError = err;
        return result;
    }

    // (3) SR-HAL-10: configure the WM8731 over /dev/i2c-0, stopping at the first missing ACK.
    const int fd = sys.openDevice(std::string(paths.i2cDevice), err);
    if (fd < 0) {
        releaseHpsI2cControl(sys, paths, gpioDir, number);
        result.status = CodecStatus::kI2cOpenFailed;
        result.sysError = err;
        return result;
    }
    err = sys.setI2cAddress(fd, kCodecI2cAddress);
    if (err != 0) {
        result.status = CodecStatus::kI2cAddressFailed;
        result.sysError = err;
    } else {
        for (const CodecWrite& write : kCodecSetup) {
            const std::array<std::uint8_t, 2> bytes = encodeCodecWrite(write);
            err = sys.writeBytes(fd, bytes.data(), bytes.size());
            if (err != 0) {
                result.status = CodecStatus::kCodecNoAck;
                result.sysError = err;
                result.reg = write.reg;
                break;
            }
        }
    }

    // (4) Close the device and give the bus back to the FPGA side.
    sys.close(fd);
    releaseHpsI2cControl(sys, paths, gpioDir, number);
    if (!result.ok()) {
        return result;
    }

    // (5) SR-HAL-10, SR-AUD-09: only now let the Tone Generator play; keep the mute bit.
    const std::uint32_t audio = regs.read32(kOffsetAudioControl);
    regs.write32(kOffsetAudioControl, audio | (1U << kBitAudioEnable));
    return result;
}

std::string describeCodecFailure(const CodecResult& result, const CodecPaths& paths) {
    const std::string sysText =
        (result.sysError != 0) ? std::string(" (") + std::strerror(result.sysError) + ")" : "";
    switch (result.status) {
        case CodecStatus::kOk:
            return "";
        case CodecStatus::kGpioChipNotFound:
            return "codec configuration error: no usable GPIO chip labelled " +
                   std::string(paths.gpioChipLabel) + " in " + std::string(paths.gpioClassDir) +
                   " for HPS_I2C_CONTROL (needs the #39 device tree)";
        case CodecStatus::kGpioSetupFailed:
            return "codec configuration error: cannot drive HPS_I2C_CONTROL (GPIO " +
                   std::to_string(result.gpio) + ") high" + sysText;
        case CodecStatus::kI2cOpenFailed:
            return "codec configuration error: cannot open " + std::string(paths.i2cDevice) +
                   sysText;
        case CodecStatus::kI2cAddressFailed:
            return "codec configuration error: cannot address the codec on " +
                   std::string(paths.i2cDevice) + sysText;
        case CodecStatus::kCodecNoAck:
            return "codec configuration error: codec did not acknowledge register R" +
                   std::to_string(result.reg) + sysText;
    }
    return "codec configuration error";
}

}  // namespace arcade::hal
