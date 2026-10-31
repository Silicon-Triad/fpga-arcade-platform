/// @file
/// App state Paused: empty until issue #10 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::Paused. Empty; issue #10 adds its behaviour.
class PausedScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
