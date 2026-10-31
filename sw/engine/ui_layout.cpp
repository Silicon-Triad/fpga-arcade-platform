#include "engine/ui_layout.hpp"

#include "assets/tile_ids.hpp"

namespace arcade {
namespace {

constexpr int kAccentSpacing = 8;  // section 3.1.1: accent tile every 8 columns
constexpr int kLastRow = kGridRows - 1;
constexpr int kLastCol = kGridCols - 1;
constexpr int kBandCol = 1;  // section 3.1.1: hint band over columns 1-126
constexpr int kBandWidth = 126;
constexpr int kHintTextCol = 2;  // same left edge as the HUD; the SRS gives no column
constexpr TileIndex kBlankTile = assets::kFontFirstTile;

}  // namespace

ApiStatus drawFrame(ArcadeApi& api) noexcept {
    api.fillRect(0, 0, kGridCols, 1, assets::kTileFrame);
    api.fillRect(0, kLastRow, kGridCols, 1, assets::kTileFrame);
    api.fillRect(0, 0, 1, kGridRows, assets::kTileFrame);
    api.fillRect(kLastCol, 0, 1, kGridRows, assets::kTileFrame);
    for (int col = 0; col < kGridCols; col += kAccentSpacing) {
        api.setTile(col, 0, assets::kTileAccent);
        api.setTile(col, kLastRow, assets::kTileAccent);
    }
    return ApiStatus::Ok;
}

ApiStatus drawHintBar(ArcadeApi& api, HintScreen screen, bool soundOn) noexcept {
    const std::optional<HintBar> bar = hintBarFor(screen, soundOn);
    if (!bar) {
        return ApiStatus::Ok;
    }
    const ApiStatus status = api.fillRect(kBandCol, bar->row, kBandWidth, 1, kBlankTile);
    if (status != ApiStatus::Ok) {
        return status;
    }
    return api.drawText(kHintTextCol, bar->row, bar->text(), TextScale::X1);
}

ApiStatus drawBox(ArcadeApi& api, int col, int row, int width, int height,
                  TileIndex frameTile) noexcept {
    // The first call range-checks the whole box, so a bad box draws nothing.
    const ApiStatus status = api.fillRect(col, row, width, height, frameTile);
    if (status != ApiStatus::Ok || width <= 2 || height <= 2) {
        return status;
    }
    return api.fillRect(col + 1, row + 1, width - 2, height - 2, kBlankTile);
}

ApiStatus drawDivider(ArcadeApi& api, int col, int row, int width) noexcept {
    return api.fillRect(col, row, width, 1, assets::kTileDivider);
}

}  // namespace arcade
