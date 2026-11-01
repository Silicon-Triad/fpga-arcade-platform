#include "engine/screens/playing_screen.hpp"

#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

constexpr int kHudRow = 1;        // section 3.1.1 common layout
constexpr int kHudNameCol = 2;    // section 3.1.1 screens 3 and 4
constexpr int kHudLastCol = 125;  // section 3.1.1 screens 3 and 4

}  // namespace

void PlayingScreen::start(Game& game, std::uint8_t gameId, std::string_view name,
                          HintScreen hint) noexcept {
    game_ = &game;
    gameId_ = gameId;
    name_ = name;
    hint_ = hint;
    game_->init();
}

void PlayingScreen::restart() noexcept {
    if (game_ != nullptr) {
        game_->init();
    }
}

void PlayingScreen::update(const InputFrame& input) noexcept {
    if (game_ != nullptr) {
        game_->update(input);
    }
}

void PlayingScreen::render(EngineApi& api) noexcept {
    if (game_ != nullptr) {
        renderPicture(api);
        drawHintBar(api, hint_, soundOn_);
    }
}

void PlayingScreen::renderPicture(EngineApi& api) noexcept {
    if (game_ == nullptr) {
        return;
    }
    api.clear(0);  // tile 0 is blank (SR-REN-07)
    drawFrame(api);
    game_->render(api);
    drawHud(api);
}

// SR-UI-04: drawn from the game's state on every tick.
void PlayingScreen::drawHud(EngineApi& api) noexcept {
    api.drawText(kHudNameCol, kHudRow, name_, TextScale::X1);
    const ScoreLine score = scoreLine("SCORE", game_->score());
    api.drawText(centredCol(static_cast<int>(score.length), TextScale::X1), kHudRow, score.text(),
                 TextScale::X1);
    if (hint_ == HintScreen::Snake) {
        const ScoreLine hi = scoreLine("HI", highScore());
        api.drawText(rightAlignedCol(static_cast<int>(hi.length), kHudLastCol), kHudRow, hi.text(),
                     TextScale::X1);
    }
    // Breakout's "LIVES n" needs Game::lives(), frozen-interface issue #92.
}

std::uint32_t PlayingScreen::highScore() const noexcept {
    if (scores_ == nullptr) {
        return 0;
    }
    const HighScoreStore::GameTable& table = scores_->table(gameId_);
    return table.count > 0 ? table.rows[0].score : 0;
}

bool PlayingScreen::scoreQualifies() const noexcept {
    return scores_ != nullptr && game_ != nullptr && scores_->qualifies(gameId_, game_->score());
}

}  // namespace arcade
