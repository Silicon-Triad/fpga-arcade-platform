/// @file
/// App state ShuttingDown: empty until issue #15 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::ShuttingDown. Empty; issue #15 adds its behaviour.
class ShuttingDownScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
