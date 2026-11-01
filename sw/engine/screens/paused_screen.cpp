#include "engine/screens/paused_screen.hpp"

#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 5.
constexpr int kBoxCol = 40;
constexpr int kBoxRow = 38;
constexpr int kBoxWidth = 48;   // columns 40-87
constexpr int kBoxHeight = 20;  // rows 38-57
constexpr int kTitleRow = 41;
constexpr int kResumeRow = 48;
constexpr int kQuitRow = 50;
constexpr int kSoundRow = 52;

}  // namespace

void PausedScreen::update(const InputFrame&) noexcept {}  // SR-UI-05: the game stays frozen

void PausedScreen::render(EngineApi& api) noexcept {
    playing_.renderPicture(api);
    drawBox(api, kBoxCol, kBoxRow, kBoxWidth, kBoxHeight, assets::kTileBoxBlue);
    drawCentred(api, kTitleRow, "PAUSED", TextScale::X4);
    drawCentred(api, kResumeRow, "P RESUME", TextScale::X1);
    drawCentred(api, kQuitRow, "ESC QUIT TO MENU", TextScale::X1);
    drawCentred(api, kSoundRow, "M SOUND ON/OFF", TextScale::X1);
    drawHintBar(api, HintScreen::Paused, playing_.soundOn());
}

}  // namespace arcade
