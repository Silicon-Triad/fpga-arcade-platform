/// @file
/// Common screen layout (srs-huzaifa-rev3.md section 3.1.1), drawn through the Arcade API.
#pragma once

#include "arcade/arcade_api.hpp"
#include "arcade/types.hpp"
#include "engine/hint_text.hpp"

namespace arcade {

/// Column that centres `length` characters on the grid (section 3.1.1 centred text).
constexpr int centredCol(int length, TextScale scale) noexcept {
    return (kGridCols - length * static_cast<int>(scale)) / 2;
}

/// Column that makes `length` scale-1 characters end at `lastCol` (section 3.1.1 HUD).
constexpr int rightAlignedCol(int length, int lastCol) noexcept { return lastCol - length + 1; }

/// Draws the border on rows 0 and 95 and columns 0 and 127, with a blue accent tile every 8
/// columns on rows 0 and 95 (section 3.1.1 common layout).
ApiStatus drawFrame(ArcadeApi& api) noexcept;

/// Blanks the hint band and writes `screen`'s hint with the sound state (SR-UI-13). Does
/// nothing on screens without a hint bar.
ApiStatus drawHintBar(ArcadeApi& api, HintScreen screen, bool soundOn) noexcept;

/// Draws a width x height box: a one-tile `frameTile` border around a blank inside (section
/// 3.1.1 screens 5, 6, 9 and 10a).
ApiStatus drawBox(ArcadeApi& api, int col, int row, int width, int height,
                  TileIndex frameTile) noexcept;

/// Draws a horizontal divider of `width` cells (section 3.1.1 screens 2, 7 and 8).
ApiStatus drawDivider(ArcadeApi& api, int col, int row, int width) noexcept;

}  // namespace arcade
