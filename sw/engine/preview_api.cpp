#include "engine/preview_api.hpp"

#include <algorithm>

namespace arcade {
namespace {

// DESIGN CHOICE: the part of the game grid shown; it holds the Snake start cell (43, 48) and
// the Breakout bricks on rows 8-19. Cells stay 1:1, so Snake keeps its 3 x 3-tile cells.
constexpr int kSourceCol = 34;
constexpr int kSourceRow = 8;
constexpr int kColShift = kPreviewFirstCol - kSourceCol;
constexpr int kRowShift = kPreviewFirstRow - kSourceRow;

}  // namespace

ApiStatus PreviewApi::setTile(int col, int row, TileIndex tile) noexcept {
    const int panelCol = col + kColShift;
    const int panelRow = row + kRowShift;
    if (panelCol < kPreviewFirstCol || panelCol > kPreviewLastCol || panelRow < kPreviewFirstRow ||
        panelRow > kPreviewLastRow) {
        return ApiStatus::Ok;
    }
    return target_.setTile(panelCol, panelRow, tile);
}

ApiStatus PreviewApi::fillRect(int col, int row, int width, int height, TileIndex tile) noexcept {
    const int left = std::max(col + kColShift, kPreviewFirstCol);
    const int right = std::min(col + width - 1 + kColShift, kPreviewLastCol);
    const int top = std::max(row + kRowShift, kPreviewFirstRow);
    const int bottom = std::min(row + height - 1 + kRowShift, kPreviewLastRow);
    if (left > right || top > bottom) {
        return ApiStatus::Ok;
    }
    return target_.fillRect(left, top, right - left + 1, bottom - top + 1, tile);
}

ApiStatus PreviewApi::drawText(int, int, std::string_view, TextScale) noexcept {
    return ApiStatus::Ok;
}

void PreviewApi::clear(TileIndex) noexcept {}

ApiStatus PreviewApi::present() noexcept { return ApiStatus::Ok; }

InputFrame PreviewApi::pollInput() noexcept { return InputFrame{}; }

void PreviewApi::playSound(EffectId) noexcept {}

void PreviewApi::setMute(bool) noexcept {}

ApiStatus PreviewApi::loadTiles(const TileSet&, const Palette&) noexcept { return ApiStatus::Ok; }

std::uint32_t PreviewApi::frameCount() const noexcept { return target_.frameCount(); }

}  // namespace arcade
