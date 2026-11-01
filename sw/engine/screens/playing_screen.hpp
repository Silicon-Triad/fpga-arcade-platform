/// @file
/// App state Playing: drives the running game through the game interface only (SR-FW-03) and
/// draws the HUD (SR-UI-04).
#pragma once

#include <cstdint>
#include <string_view>

#include "arcade/game.hpp"
#include "engine/high_score_store.hpp"
#include "engine/hint_text.hpp"
#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::Playing (SR-FW-03, SR-UI-03, SR-UI-04).
class PlayingScreen final : public Screen {
  public:
    /// `soundOn` is the engine's sound state and must outlive the screen (SR-FW-11).
    explicit PlayingScreen(const bool& soundOn) noexcept : soundOn_(soundOn) {}

    /// The table behind HI and the top-10 check; nullptr means none (HI 0, nothing qualifies).
    void setScores(const HighScoreStore* scores) noexcept { scores_ = scores; }
    /// Starts a new round of `game`; `hint` picks the hint bar (SR-FW-03, SR-UI-03).
    void start(Game& game, std::uint8_t gameId, std::string_view name, HintScreen hint) noexcept;
    /// Starts a new round of the same game (SR-UI-09).
    void restart() noexcept;
    void update(const InputFrame& input) noexcept override;
    void render(EngineApi& api) noexcept override;
    /// Draws the frame, the game and the HUD, without a hint bar. Paused and GameOver draw their
    /// box over this picture (SR-UI-05, SR-UI-09). Repeat calls play no sound (SR-FW-03).
    void renderPicture(EngineApi& api) noexcept;
    /// The running game, or nullptr.
    const Game* game() const noexcept { return game_; }
    /// The game's current top score (SR-UI-04).
    std::uint32_t highScore() const noexcept;
    /// True if the running game's score enters its top 10 (SR-UI-08).
    bool scoreQualifies() const noexcept;
    /// Current sound state for hint bars (SR-UI-13).
    bool soundOn() const noexcept { return soundOn_; }

  private:
    void drawHud(EngineApi& api) noexcept;

    const bool& soundOn_;
    Game* game_ = nullptr;
    const HighScoreStore* scores_ = nullptr;
    std::uint8_t gameId_ = 0;
    std::string_view name_;
    HintScreen hint_ = HintScreen::Snake;
};

}  // namespace arcade
