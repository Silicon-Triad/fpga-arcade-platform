#include "engine/screens/keyboard_lost_screen.hpp"

#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 9.
constexpr int kBoxCol = 30;
constexpr int kBoxRow = 38;
constexpr int kBoxWidth = 68;   // columns 30-97
constexpr int kBoxHeight = 20;  // rows 38-57
constexpr int kTitleRow = 41;
constexpr int kPlugRow = 49;
constexpr int kPausedRow = 52;

}  // namespace

void KeyboardLostScreen::update(const InputFrame&) noexcept {}  // SR-UI-11: the game stays frozen

void KeyboardLostScreen::render(EngineApi& api) noexcept {
    playing_.renderPicture(api);
    drawBox(api, kBoxCol, kBoxRow, kBoxWidth, kBoxHeight, assets::kTileBoxRed);
    drawCentred(api, kTitleRow, "NO KEYBOARD", TextScale::X4);
    drawCentred(api, kPlugRow, "PLUG IN A USB KEYBOARD TO CONTINUE", TextScale::X1);
    drawCentred(api, kPausedRow, "GAME PAUSED", TextScale::X1);
    drawHintBar(api, HintScreen::NoKeyboard, playing_.soundOn());
}

}  // namespace arcade
