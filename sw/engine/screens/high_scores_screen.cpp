#include "engine/screens/high_scores_screen.hpp"

#include <array>
#include <cstdio>
#include <string_view>

#include "assets/game_tiles.hpp"
#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 8; the divider columns and the rank alignment are measured from Figure 3.8.
constexpr std::string_view kTitle = "HIGH SCORES";
constexpr int kTitleRow = 6;
constexpr int kTitleDividerRow = 16;
constexpr int kTitleDividerCol = 16;
constexpr int kTitleDividerWidth = 96;
constexpr int kSelectorRow = 20;
constexpr int kPrevCol = 40;
constexpr int kNextCol = 84;
constexpr int kHeadingRow = 30;
constexpr int kRankCol = 36;
constexpr int kRankLastCol = 38;
constexpr int kNameCol = 56;
constexpr int kScoreCol = 80;
constexpr int kTableDividerRow = 32;
constexpr int kTableCol = 34;
constexpr int kTableWidth = 60;
constexpr int kFirstEntryRow = 34;
constexpr int kEntrySpacing = 2;
constexpr std::size_t kRankTextMax = 4;
constexpr std::size_t kScoreTextMax = 8;

}  // namespace

void HighScoresScreen::setSource(const std::vector<MenuGame>* games,
                                 const HighScoreStore* scores) noexcept {
    games_ = games;
    scores_ = scores;
}

void HighScoresScreen::open() noexcept {
    index_ = 0;
    highlightRank_ = kNoHighlight;
}

void HighScoresScreen::open(std::uint8_t gameId, std::uint8_t rank) noexcept {
    index_ = 0;
    for (std::size_t i = 0; games_ != nullptr && i < games_->size(); ++i) {
        if ((*games_)[i].row->game_id == gameId) {
            index_ = i;
        }
    }
    highlightGame_ = gameId;
    highlightRank_ = rank;
}

void HighScoresScreen::update(const InputFrame& input) noexcept {
    if (games_ == nullptr || games_->empty()) {
        return;
    }
    const std::size_t count = games_->size();
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type != InputEventType::KeyDown) {
            continue;
        }
        if (event.action == Action::Left) {  // SR-UI-12: wraps at both ends
            index_ = (index_ + count - 1) % count;
        } else if (event.action == Action::Right) {
            index_ = (index_ + 1) % count;
        }
    }
}

void HighScoresScreen::render(EngineApi& api) noexcept {
    api.clear(assets::kFontFirstTile);
    drawFrame(api);
    drawCentred(api, kTitleRow, kTitle, TextScale::X4);
    drawDivider(api, kTitleDividerCol, kTitleDividerRow, kTitleDividerWidth);
    api.drawText(kRankCol, kHeadingRow, "RANK", TextScale::X1);
    api.drawText(kNameCol, kHeadingRow, "NAME", TextScale::X1);
    api.drawText(kScoreCol, kHeadingRow, "SCORE", TextScale::X1);
    drawDivider(api, kTableCol, kTableDividerRow, kTableWidth);
    drawHintBar(api, HintScreen::HighScores, soundOn_);  // SR-UI-13
    if (games_ == nullptr || games_->empty()) {
        return;
    }

    const GameRegistryRow& game = *(*games_)[index_].row;
    api.drawText(kPrevCol, kSelectorRow, "<", TextScale::X4);
    drawCentred(api, kSelectorRow, game.name, TextScale::X4);
    api.drawText(kNextCol, kSelectorRow, ">", TextScale::X4);
    if (scores_ == nullptr) {
        return;
    }
    const HighScoreStore::GameTable& table = scores_->table(game.game_id);
    for (std::size_t i = 0; i < table.count; ++i) {  // SR-UI-12: rank, initials, score
        const HighScoreRow& entry = table.rows[i];
        const int row = kFirstEntryRow + static_cast<int>(i) * kEntrySpacing;
        // SR-UI-10: section 3.1.1 gives no highlight, so it uses the menu's band and green bar.
        if (game.game_id == highlightGame_ && entry.rank == highlightRank_) {
            api.fillRect(kTableCol, row, kTableWidth, 1, assets::kTileMenuBand);
            api.setTile(kTableCol, row, assets::kTileMenuGreen);
        }
        std::array<char, kRankTextMax> rank{};
        const int rankLength =
            std::snprintf(rank.data(), rank.size(), "%u", static_cast<unsigned>(entry.rank));
        api.drawText(rightAlignedCol(rankLength, kRankLastCol), row,
                     {rank.data(), static_cast<std::size_t>(rankLength)}, TextScale::X1);
        api.drawText(kNameCol, row, {entry.initials.data(), entry.initials.size()}, TextScale::X1);
        std::array<char, kScoreTextMax> score{};
        const int scoreLength =
            std::snprintf(score.data(), score.size(), "%06u", static_cast<unsigned>(entry.score));
        api.drawText(kScoreCol, row, {score.data(), static_cast<std::size_t>(scoreLength)},
                     TextScale::X1);
    }
}

}  // namespace arcade
