/// @file
/// App state ShutdownConfirm: empty until issue #15 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::ShutdownConfirm. Empty; issue #15 adds its behaviour.
class ShutdownConfirmScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
