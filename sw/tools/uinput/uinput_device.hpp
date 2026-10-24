#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arcade::uinput {

/// Default uinput device path. The SRS names uinput (PR-06, glossary) but not its path; this
/// is the path in the kernel's uinput documentation, chosen by the area owner in #23
/// (sw/tools/uinput/README.md). Both programs accept `--device` to use another one.
constexpr const char* kDefaultUinputPath = "/dev/uinput";

/// A virtual keyboard created through Linux uinput, used to inject key events for PR-06.
/// Destroying the object removes the device.
class UinputDevice {
  public:
    UinputDevice() = default;
    ~UinputDevice();
    UinputDevice(const UinputDevice&) = delete;
    UinputDevice& operator=(const UinputDevice&) = delete;
    UinputDevice(UinputDevice&&) = delete;
    UinputDevice& operator=(UinputDevice&&) = delete;

    /// Opens `uinputPath` and creates a keyboard called `name` that reports every key in
    /// `keys` (EV_KEY). Returns false and sets error() on failure. The kernel then creates a
    /// `/dev/input/eventN` node for it; that takes a moment (see input_throughput_test).
    bool create(const std::string& uinputPath, const std::string& name,
                const std::vector<std::uint16_t>& keys);

    /// Sends one EV_KEY event (value 1 = press, 0 = release) followed by SYN_REPORT, as a
    /// keyboard does. Returns false and sets error() on failure.
    bool sendKey(std::uint16_t code, std::int32_t value);

    /// What failed, with the system error text.
    const std::string& error() const { return error_; }

  private:
    bool fail(const std::string& what);
    bool write(std::uint16_t type, std::uint16_t code, std::int32_t value);

    int fd_ = -1;
    bool created_ = false;
    std::string error_;
};

}  // namespace arcade::uinput
