/// @file
/// App state HighScores: section 3.1.1 screen 8 (SR-UI-12).
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/game_menu.hpp"
#include "engine/high_score_store.hpp"
#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::HighScores: the top 10 of one game; Left and Right switch game and wrap
/// (SR-UI-12). App handles Back.
class HighScoresScreen final : public Screen {
  public:
    /// Rank value meaning no row is highlighted.
    static constexpr std::uint8_t kNoHighlight = 0;

    /// `soundOn` is the engine's sound state and must outlive the screen (SR-FW-11).
    explicit HighScoresScreen(const bool& soundOn) noexcept : soundOn_(soundOn) {}

    /// The games, in menu order, and their scores. Both must outlive the screen; `scores` may be
    /// nullptr (no rows).
    void setSource(const std::vector<MenuGame>* games, const HighScoreStore* scores) noexcept;
    /// Shows the first game, with no highlight (SR-UI-12).
    void open() noexcept;
    /// Shows `gameId` with row `rank` highlighted (SR-UI-10).
    void open(std::uint8_t gameId, std::uint8_t rank) noexcept;
    /// Handles Left and Right (SR-UI-12).
    void update(const InputFrame& input) noexcept override;
    /// Draws section 3.1.1 screen 8.
    void render(EngineApi& api) noexcept override;

    /// Index into the games of the game shown.
    std::size_t gameIndex() const noexcept { return index_; }

  private:
    const bool& soundOn_;
    const std::vector<MenuGame>* games_ = nullptr;
    const HighScoreStore* scores_ = nullptr;
    std::size_t index_ = 0;
    std::uint8_t highlightGame_ = 0;
    std::uint8_t highlightRank_ = kNoHighlight;
};

}  // namespace arcade
