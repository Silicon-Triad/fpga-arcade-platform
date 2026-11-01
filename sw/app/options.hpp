/// @file
/// Command-line options of the arcade program (SR-FW-05).
#pragma once

#include <cstdint>
#include <optional>

namespace arcade {

/// Rendering backend chosen at start-up (SR-FW-05).
enum class BackendKind : std::uint8_t { Sdl, Fpga };

/// Parsed command line.
struct Options {
    BackendKind backend = BackendKind::Sdl;  ///< --backend=sdl|fpga (SR-FW-05).
    bool profile = false;                    ///< --profile: game-loop statistics (PR-04).
};

/// Parses argv[1..]. `defaultBackend` applies without --backend (SR-FW-05).
/// @return nullopt for an unknown option or backend.
std::optional<Options> parseOptions(int argc, const char* const* argv,
                                    BackendKind defaultBackend) noexcept;

}  // namespace arcade
