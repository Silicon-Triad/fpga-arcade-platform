#include "engine/screens/game_over_screen.hpp"

#include <string_view>

#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 6.
constexpr int kBoxCol = 36;
constexpr int kBoxRow = 34;
constexpr int kBoxWidth = 56;   // columns 36-91
constexpr int kBoxHeight = 28;  // rows 34-61
constexpr int kTitleRow = 37;
constexpr int kScoreRow = 45;
constexpr int kHiRow = 47;
constexpr int kAgainRow = 52;
constexpr int kMenuRow = 54;

void drawCentred(EngineApi& api, int row, std::string_view text, TextScale scale) noexcept {
    api.drawText(centredCol(static_cast<int>(text.size()), scale), row, text, scale);
}

}  // namespace

void GameOverScreen::update(const InputFrame&) noexcept {}

void GameOverScreen::render(EngineApi& api) noexcept {
    const Game* game = playing_.game();
    if (game == nullptr) {
        return;
    }
    playing_.renderPicture(api);
    drawBox(api, kBoxCol, kBoxRow, kBoxWidth, kBoxHeight, assets::kTileBoxRed);
    drawCentred(api, kTitleRow, "GAME OVER", TextScale::X4);
    const ScoreLine score = scoreLine("SCORE", game->score());
    drawCentred(api, kScoreRow, score.text(), TextScale::X1);  // SR-UI-09
    const ScoreLine hi = scoreLine("HI", playing_.highScore());
    drawCentred(api, kHiRow, hi.text(), TextScale::X1);
    drawCentred(api, kAgainRow, "ENTER PLAY AGAIN", TextScale::X1);
    drawCentred(api, kMenuRow, "ESC MAIN MENU", TextScale::X1);
    drawHintBar(api, HintScreen::GameOver, playing_.soundOn());
}

}  // namespace arcade
