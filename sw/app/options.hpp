/// @file
/// Command-line options of the arcade program (SR-FW-05).
#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace arcade {

/// Rendering backend chosen at start-up (SR-FW-05).
enum class BackendKind : std::uint8_t { Sdl, Fpga };

/// Parsed command line.
struct Options {
    BackendKind backend = BackendKind::Sdl;  ///< --backend=sdl|fpga (SR-FW-05).
    bool profile = false;                    ///< --profile: game-loop statistics (PR-04).
    bool latency = false;  ///< --latency: key-to-swap latency, FPGA backend only (PR-02, #47).
    /// --usb-us=N: the keyboard's USB polling interval in microseconds (part a of the PR-02
    /// bound), worked out from bInterval and the device speed (sw/app/README.md). Only used
    /// with --latency.
    std::uint32_t usbUs = 0;
};

/// DESIGN CHOICE, NOT AN SRS VALUE: accepted --usb-us range. Interrupt endpoint polling
/// intervals (USB 2.0 spec, 9.6.6): low and full speed, bInterval x 1 ms (at most 255 ms);
/// high speed, 2^(bInterval-1) x 125 us with bInterval 1..16 (125 us to 4,096 ms).
constexpr std::uint32_t kMinUsbUs = 125;
constexpr std::uint32_t kMaxUsbUs = 4096000;

/// Parses argv[1..]. `defaultBackend` applies without --backend (SR-FW-05).
/// @return nullopt for an unknown option or backend, a bad --usb-us, or --latency without
/// --backend=fpga and --usb-us (PR-02).
std::optional<Options> parseOptions(int argc, const char* const* argv,
                                    BackendKind defaultBackend) noexcept;

}  // namespace arcade
