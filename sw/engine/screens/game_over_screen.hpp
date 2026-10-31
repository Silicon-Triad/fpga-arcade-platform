/// @file
/// App state GameOver: empty until issue #10 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::GameOver. Empty; issue #10 adds its behaviour.
class GameOverScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
