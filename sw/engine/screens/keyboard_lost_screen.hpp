/// @file
/// App state KeyboardLost: empty until issue #13 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::KeyboardLost. Empty; issue #13 adds its behaviour.
class KeyboardLostScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
