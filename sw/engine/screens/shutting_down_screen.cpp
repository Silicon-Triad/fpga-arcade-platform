#include "engine/screens/shutting_down_screen.hpp"

#include <string_view>

#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 10b: scale 4 on rows 46-49, columns 28-99.
constexpr std::string_view kMessage = "SAFE TO SWITCH OFF";
constexpr int kMessageRow = 46;

}  // namespace

void ShuttingDownScreen::update(const InputFrame&) noexcept {}

void ShuttingDownScreen::render(EngineApi& api) noexcept {
    if (!saveFinished_) {
        return;
    }
    api.clear(assets::kFontFirstTile);  // palette entry 0, no frame (section 3.1.1 screen 10b)
    api.drawText(centredCol(static_cast<int>(kMessage.size()), TextScale::X4), kMessageRow,
                 kMessage, TextScale::X4);
}

}  // namespace arcade
