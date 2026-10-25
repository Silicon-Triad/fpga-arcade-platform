/// @file
/// The game loop: one tick per displayed frame, pollInput -> update -> render -> present
/// (SR-FW-02), with optional tick-time profiling (PR-03, PR-04).
#pragma once

#include <cstdint>

#include "arcade/engine_api.hpp"
#include "engine/screen.hpp"
#include "engine/tick_clock.hpp"
#include "engine/tick_profiler.hpp"

namespace arcade {

/// Game-loop options. #16 wires the `--profile` command-line flag to `profile`.
struct GameLoopConfig {
    bool profile = false;  ///< Time each tick and keep PR-04 statistics.
};

/// Everything a profiling run reports (PR-03, PR-04).
struct GameLoopReport {
    std::uint64_t ticks = 0;            ///< Ticks run (PR-03).
    std::uint32_t frameCountStart = 0;  ///< frameCount() when the loop was created (PR-03).
    std::uint32_t frameCountEnd = 0;    ///< frameCount() now (PR-03).
    std::uint64_t presentErrors = 0;    ///< present() calls that returned an error.
    TickStats tickStats;                ///< Tick-time statistics; zero unless profiling (PR-04).
};

/// Runs the active screen once per displayed frame (SR-FW-02). Works only through EngineApi,
/// never the HAL (QA-01). No call throws (R-4.1.5).
class GameLoop {
  public:
    /// Creates the loop and records frameCount() as the start value (PR-03).
    GameLoop(EngineApi& api, TickClock& clock, GameLoopConfig config = {});

    /// Makes `screen` the active screen from the next tick; nullptr means none (SR-FW-02).
    void setScreen(Screen* screen) noexcept;

    /// Runs one tick: pollInput(), the active screen's update() and render(), then present(),
    /// which waits for the display refresh (SR-FW-02). When profiling, the work before
    /// present() is timed (PR-04). If present() fails, the error is counted, the tick still
    /// counts, and the next tick runs normally (R-4.1.5).
    void runTick() noexcept;

    /// Runs ticks until `shouldStop()` returns true; checked before each tick.
    template <typename StopFn>
    void run(StopFn shouldStop) noexcept {
        while (!shouldStop()) {
            runTick();
        }
    }

    /// Number of ticks run (PR-03).
    std::uint64_t tickCount() const noexcept { return ticks_; }
    /// frameCount() when the loop was created (PR-03).
    std::uint32_t startFrameCount() const noexcept { return frameCountStart_; }
    /// Number of present() calls that returned an error.
    std::uint64_t presentErrors() const noexcept { return presentErrors_; }

    /// Tick count, frameCount() at start and now, and PR-04 statistics if profiling. Call
    /// outside the tick (it sorts a copy of the durations).
    GameLoopReport report() const;

  private:
    EngineApi& api_;
    TickClock& clock_;
    GameLoopConfig config_;
    Screen* screen_ = nullptr;
    TickProfiler profiler_;
    std::uint64_t ticks_ = 0;
    std::uint64_t presentErrors_ = 0;
    std::uint32_t frameCountStart_ = 0;
};

}  // namespace arcade
