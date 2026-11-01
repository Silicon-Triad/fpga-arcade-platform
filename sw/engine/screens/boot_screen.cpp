#include "engine/screens/boot_screen.hpp"

#include <algorithm>
#include <string_view>

#include "assets/game_tiles.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Text colours (yellow, red) wait for #81.
// Section 3.1.1 screen 1.
constexpr std::string_view kLogo = "FPGA ARCADE";
// Section 3.1.1 screen 1.
constexpr std::string_view kSubtitle = "SILICON TRIAD - DE1-SOC - 1024X768";
// Section 3.1.1 screen 1.
constexpr std::string_view kStatus = "STARTING...";
// Section 3.1.1 screen 1.
constexpr std::string_view kNoKeyboard = "NO KEYBOARD FOUND - PLUG IN A USB KEYBOARD";
// Section 3.1.1 screen 1.
constexpr int kLogoRow = 34;
// Section 3.1.1 screen 1.
constexpr int kSubtitleRow = 44;
// Section 3.1.1 screen 1.
constexpr int kBarRow = 50;
// Section 3.1.1 screen 1.
constexpr int kBarFirstCol = 32;
// Section 3.1.1 screen 1.
constexpr int kBarLastCol = 95;
constexpr int kBarWidth = kBarLastCol - kBarFirstCol + 1;
// Section 3.1.1 screen 1.
constexpr int kStatusRow = 53;
// Section 3.1.1 screen 1.
constexpr int kNoKeyboardRow = 60;

}  // namespace

void BootScreen::setProgress(int done, int total) noexcept {
    barCells_ = total > 0 ? std::clamp(done, 0, total) * kBarWidth / total : 0;
}

void BootScreen::update(const InputFrame&) noexcept {}

void BootScreen::render(EngineApi& api) noexcept {
    api.clear(assets::kFontFirstTile);
    (void)drawFrame(api);  // Section 3.1.1 common layout: every screen but 10b has the frame.
    drawCentred(api, kLogoRow, kLogo, TextScale::X4);
    drawCentred(api, kSubtitleRow, kSubtitle, TextScale::X1);
    if (barCells_ > 0) {
        api.fillRect(kBarFirstCol, kBarRow, barCells_, 1, assets::kTileMenuGreen);
    }
    drawCentred(api, kStatusRow, kStatus, TextScale::X1);
    if (keyboardMissing_) {
        drawCentred(api, kNoKeyboardRow, kNoKeyboard, TextScale::X1);
    }
}

}  // namespace arcade
