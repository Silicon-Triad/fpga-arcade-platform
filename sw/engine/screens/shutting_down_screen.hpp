/// @file
/// App state ShuttingDown: section 3.1.1 screen 10b (SR-FW-13, SF-07).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// "SAFE TO SWITCH OFF". Draws nothing until saveFinished(), so the message can never appear
/// while a save is still pending (SR-FW-13, SF-07). No keys work.
class ShuttingDownScreen final : public Screen {
  public:
    /// The pending high-score save is done; render() may now draw screen 10b (SR-FW-13).
    void saveFinished() noexcept { saveFinished_ = true; }

    void update(const InputFrame& input) noexcept override;
    /// Draws screen 10b once the save has finished; until then the last frame stays.
    void render(EngineApi& api) noexcept override;

  private:
    bool saveFinished_ = false;
};

}  // namespace arcade
