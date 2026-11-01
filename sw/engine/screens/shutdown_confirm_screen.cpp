#include "engine/screens/shutdown_confirm_screen.hpp"

#include <string_view>

#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 10a.
constexpr int kBoxCol = 40;
constexpr int kBoxRow = 38;
constexpr int kBoxWidth = 48;   // columns 40-87
constexpr int kBoxHeight = 20;  // rows 38-57
constexpr int kTitleRow = 41;
constexpr int kYesRow = 48;
constexpr int kNoRow = 50;

void drawCentred(EngineApi& api, int row, std::string_view text, TextScale scale) noexcept {
    api.drawText(centredCol(static_cast<int>(text.size()), scale), row, text, scale);
}

}  // namespace

void ShutdownConfirmScreen::update(const InputFrame& input) noexcept {
    for (std::size_t i = 0; i < input.count && request_ == Request::None; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type != InputEventType::KeyDown) {
            continue;
        }
        if (event.action == Action::Select) {
            request_ = Request::Confirm;  // SR-UI-14
        } else if (event.action == Action::Back) {
            request_ = Request::Cancel;  // SR-UI-14
        }
    }
}

void ShutdownConfirmScreen::render(EngineApi& api) noexcept {
    drawBox(api, kBoxCol, kBoxRow, kBoxWidth, kBoxHeight, assets::kTileBoxBlue);
    drawCentred(api, kTitleRow, "SHUT DOWN?", TextScale::X4);
    drawCentred(api, kYesRow, "ENTER YES", TextScale::X1);
    drawCentred(api, kNoRow, "ESC NO", TextScale::X1);
    drawHintBar(api, HintScreen::ShutdownConfirm, true);  // this hint shows no sound state
}

ShutdownConfirmScreen::Request ShutdownConfirmScreen::takeRequest() noexcept {
    const Request request = request_;
    request_ = Request::None;
    return request;
}

}  // namespace arcade
