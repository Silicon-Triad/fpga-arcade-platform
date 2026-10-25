// Game loop (SR-FW-02) with tick instrumentation (PR-03, PR-04).
#include "engine/game_loop.hpp"

namespace arcade {

GameLoop::GameLoop(EngineApi& api, TickClock& clock, GameLoopConfig config)
    : api_(api),
      clock_(clock),
      config_(config),
      profiler_(config.profile ? kProfileCapacity : 0),
      frameCountStart_(api.frameCount()) {}

void GameLoop::setScreen(Screen* screen) noexcept { screen_ = screen; }

void GameLoop::runTick() noexcept {
    // PR-04: tick work is pollInput, update and render (the cell copy happens in present()).
    const std::uint64_t start = config_.profile ? clock_.nowUs() : 0;

    // SR-FW-02: pollInput(), the active screen's update(), render(), then present().
    const InputFrame input = api_.pollInput();
    if (screen_ != nullptr) {
        screen_->update(input);
        screen_->render(api_);
    }
    if (config_.profile) {
        profiler_.record(clock_.nowUs() - start);
    }

    // SR-FW-02: present() returns at the next display refresh, locking the loop to vsync.
    if (api_.present() != ApiStatus::Ok) {
        ++presentErrors_;  // R-4.1.5: no throw; the next tick runs normally.
    }
    ++ticks_;
}

GameLoopReport GameLoop::report() const {
    GameLoopReport r;
    r.ticks = ticks_;
    r.frameCountStart = frameCountStart_;
    r.frameCountEnd = api_.frameCount();
    r.presentErrors = presentErrors_;
    if (config_.profile) {
        r.tickStats = profiler_.stats();
    }
    return r;
}

}  // namespace arcade
