/// @file
/// The start-up sequence and the run of the game loop (SR-FW-01, SF-02).
#pragma once

#include <cstdio>
#include <functional>
#include <memory>
#include <string_view>

#include "app/options.hpp"
#include "arcade/engine_api.hpp"

namespace arcade {

/// Message printed when the backend cannot start (SF-02, SR-HAL-09).
constexpr std::string_view kHardwareNotReady = "graphics hardware not ready";
/// Exit status after SF-02's message.
constexpr int kExitHardwareNotReady = 1;
/// Failed step when the backend gives no other reason (SF-02).
constexpr std::string_view kStepStartBackend = "start backend";
/// Failed step when the tile set does not build (SF-02).
constexpr std::string_view kStepBuildTiles = "build tile set";
/// Failed step when loadTiles() fails (SF-02).
constexpr std::string_view kStepLoadTiles = "load tiles and palette";

/// A started backend, or the step that failed (SF-02).
struct BackendStart {
    std::unique_ptr<EngineApi> api;       ///< nullptr if the start failed.
    std::function<bool()> stopRequested;  ///< True when the program should end; may be empty.
    /// False if no keyboard was found at start-up (SR-INP-16); empty means one is always there,
    /// as on the PC. A later change arrives as KeyboardLost or KeyboardFound (SR-UI-11).
    std::function<bool()> keyboardPresent;
    std::string_view failedStep = kStepStartBackend;  ///< Used when `api` is nullptr.
    /// Ticks to keep the last frame after a confirmed shutdown, before release. For the PC only,
    /// where release closes the window; on the board the frame stays until power-off (SR-HAL-07).
    int finalFrameTicks = 0;
};

/// Starts the backend of `options` (SR-FW-05). Injected so tests use a fake backend.
using BackendFactory = std::function<BackendStart(const Options& options)>;

/// Starts the program in SR-FW-01 order (start backend, build the tile set, loadTiles once,
/// splash, menu) and runs the game loop until a shutdown is confirmed (SR-FW-13), `stopSignal`
/// returns true (SR-FW-14) or the backend asks to stop, then runs finishShutdown(). `stopSignal`
/// may be empty. Display enable is the backend's job (SR-HAL-13); destroying the backend must
/// leave it set (SR-HAL-07). On failure prints the SF-02 message and failed step to `log`.
/// @return 0, kExitShutdownRequested (SR-FW-13) or kExitHardwareNotReady (SF-02).
int runArcade(const Options& options, const BackendFactory& factory, std::FILE* log,
              const std::function<bool()>& stopSignal = {});

}  // namespace arcade
