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
    /// --usb-ms=N: the keyboard's USB polling interval in ms, from bInterval in `lsusb -v`
    /// (part a of the PR-02 bound). Only used with --latency.
    std::uint32_t usbMs = 0;
};

/// DESIGN CHOICE: largest accepted --usb-ms (bInterval for full-speed interrupt endpoints is
/// 1..255 frames of 1 ms).
constexpr std::uint32_t kMaxUsbMs = 255;

/// Parses argv[1..]. `defaultBackend` applies without --backend (SR-FW-05).
/// @return nullopt for an unknown option or backend, a bad --usb-ms, or --latency without
/// --backend=fpga and --usb-ms (PR-02).
std::optional<Options> parseOptions(int argc, const char* const* argv,
                                    BackendKind defaultBackend) noexcept;

}  // namespace arcade
