/// @file
/// Common screen layout (srs-huzaifa-rev3.md section 3.1.1), drawn through the Arcade API.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

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

/// A label, a space and a 6-digit score, in a fixed buffer: no heap in a tick (R-4.1.6).
struct ScoreLine {
    std::array<char, 16> chars{};
    std::size_t length = 0;

    /// The text as a view into this object.
    std::string_view text() const noexcept { return {chars.data(), length}; }
};

/// Formats "SCORE 000120", clamping `score` to SCORE_MAX (SR-UI-04).
ScoreLine scoreLine(std::string_view label, std::uint32_t score) noexcept;

/// Draws `text` centred on the grid at `row` (section 3.1.1 centred text).
ApiStatus drawCentred(ArcadeApi& api, int row, std::string_view text, TextScale scale) noexcept;

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
