/// @file
/// App state Playing: drives the running game through the game interface only (SR-FW-03).
/// HUD, pause and game end arrive with issue #10.
#pragma once

#include "arcade/game.hpp"
#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::Playing (SR-FW-03, SR-UI-03).
class PlayingScreen final : public Screen {
  public:
    /// Starts a new round of `game` (SR-FW-03, SR-UI-03).
    void start(Game& game) noexcept;
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
    /// The running game, or nullptr.
    const Game* game() const noexcept { return game_; }

  private:
    Game* game_ = nullptr;
};

}  // namespace arcade
