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
    /// `soundOn` is the engine's sound state and must outlive the screen (SR-FW-11).
    explicit PlayingScreen(const bool& soundOn) noexcept : soundOn_(soundOn) {}

    /// Starts a new round of `game` (SR-FW-03, SR-UI-03).
    void start(Game& game) noexcept;
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
    /// The running game, or nullptr.
    const Game* game() const noexcept { return game_; }

    /// Current sound state for hint bars (SR-UI-13).
    bool soundOn() const noexcept { return soundOn_; }

  private:
    const bool& soundOn_;
    Game* game_ = nullptr;
};

}  // namespace arcade
