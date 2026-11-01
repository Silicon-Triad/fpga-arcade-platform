/// @file
/// App state GameOver: the Game Over box over the final game picture (SR-UI-09, section 3.1.1
/// screen 6).
#pragma once

#include "engine/screen.hpp"
#include "engine/screens/playing_screen.hpp"

namespace arcade {

/// Screen of AppState::GameOver. App handles its keys (SR-UI-09).
class GameOverScreen final : public Screen {
  public:
    /// `playing` draws the picture under the box and holds the final score.
    explicit GameOverScreen(PlayingScreen& playing) noexcept : playing_(playing) {}
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;

  private:
    PlayingScreen& playing_;
};

}  // namespace arcade
