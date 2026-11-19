#include "app/options.hpp"

#include <string_view>

namespace arcade {
namespace {

constexpr std::string_view kUsbUsPrefix = "--usb-us=";

// Decimal kMinUsbUs..kMaxUsbUs.
bool parseUs(std::string_view text, std::uint32_t& out) noexcept {
    if (text.empty() || text.size() > 7) {
        return false;
    }
    std::uint32_t v = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
        v = v * 10 + static_cast<std::uint32_t>(c - '0');
    }
    if (v < kMinUsbUs || v > kMaxUsbUs) {
        return false;
    }
    out = v;
    return true;
}

}  // namespace

std::optional<Options> parseOptions(int argc, const char* const* argv,
                                    BackendKind defaultBackend) noexcept {
    Options options;
    options.backend = defaultBackend;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--backend=sdl") {  // SR-FW-05
            options.backend = BackendKind::Sdl;
        } else if (arg == "--backend=fpga") {  // SR-FW-05
            options.backend = BackendKind::Fpga;
        } else if (arg == "--profile") {
            options.profile = true;
        } else if (arg == "--latency") {  // PR-02, #47
            options.latency = true;
        } else if (arg.substr(0, kUsbUsPrefix.size()) == kUsbUsPrefix) {
            if (!parseUs(arg.substr(kUsbUsPrefix.size()), options.usbUs)) {
                return std::nullopt;
            }
        } else {
            return std::nullopt;
        }
    }
    // PR-02 (#47): --latency needs evdev (the FPGA backend) and part a from --usb-us.
    if (options.latency && (options.backend != BackendKind::Fpga || options.usbUs == 0)) {
        return std::nullopt;
    }
    return options;
}

}  // namespace arcade
