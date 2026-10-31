/// @file
/// App state HighScores: empty until issue #12 fills it (App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::HighScores. Empty; issue #12 adds its behaviour.
class HighScoresScreen final : public Screen {
  public:
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
};

}  // namespace arcade
