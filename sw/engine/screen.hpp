/// @file
/// Engine-internal screen interface: the "active screen" the game loop drives each tick
/// (SR-FW-02). Not a frozen interface; real screens arrive with issues #8 to #16.
#pragma once

#include "arcade/engine_api.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// One screen of the application (menu, game, pause, ...), driven by GameLoop (SR-FW-02).
/// No call throws (R-4.1.5).
class Screen {
  public:
    virtual ~Screen() = default;

    /// Advances the screen by one tick using this tick's input (SR-FW-02).
    virtual void update(const InputFrame& input) noexcept = 0;

    /// Draws the screen through `api`; does not call present() (SR-FW-02).
    virtual void render(EngineApi& api) noexcept = 0;
};

}  // namespace arcade
