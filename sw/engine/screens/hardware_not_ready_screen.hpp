/// @file
/// App state HardwareNotReady: empty until issue #16 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::HardwareNotReady. Empty; issue #16 adds its behaviour.
class HardwareNotReadyScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
