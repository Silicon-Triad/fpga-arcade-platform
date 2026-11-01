/// @file
/// App state Paused: the Paused box over the frozen game picture (SR-UI-05, section 3.1.1
/// screen 5).
#pragma once

#include "engine/screen.hpp"
#include "engine/screens/playing_screen.hpp"

namespace arcade {

/// Screen of AppState::Paused. App handles its keys; the game gets no updates (SR-UI-05).
class PausedScreen final : public Screen {
  public:
    /// `playing` draws the picture under the box.
    explicit PausedScreen(PlayingScreen& playing) noexcept : playing_(playing) {}
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;

  private:
    PlayingScreen& playing_;
};

}  // namespace arcade
