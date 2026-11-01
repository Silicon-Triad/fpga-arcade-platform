/// @file
/// App state KeyboardLost: the Keyboard Disconnected box over the frozen game picture (SR-UI-11,
/// section 3.1.1 screen 9).
#pragma once

#include "engine/screen.hpp"
#include "engine/screens/playing_screen.hpp"

namespace arcade {

/// Screen of AppState::KeyboardLost. No keys work and the game gets no updates (SR-UI-11).
class KeyboardLostScreen final : public Screen {
  public:
    /// `playing` draws the picture under the box.
    explicit KeyboardLostScreen(PlayingScreen& playing) noexcept : playing_(playing) {}
    void update(const InputFrame& input) noexcept override;
    /// Draws section 3.1.1 screen 9.
    void render(EngineApi& api) noexcept override;

  private:
    PlayingScreen& playing_;
};

}  // namespace arcade
