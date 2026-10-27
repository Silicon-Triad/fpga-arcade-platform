/// @file
/// Text-layout hook used by the SDL2 backend's drawText(). Character-to-tile mapping and the
/// font and mosaic tiles are issue #9 (SR-FW-12).
#pragma once

#include <array>
#include <string_view>

#include "arcade/types.hpp"

namespace arcade {

/// A full 128 x 96 tilemap, indexed [row][col] (TBL-Tilemap, SR-FW-06).
using Tilemap = std::array<std::array<TileIndex, kGridCols>, kGridRows>;

/// Writes the tiles of a text string into a tilemap (SR-FW-12).
class TextLayout {
  public:
    virtual ~TextLayout() = default;

    /// Writes `text` into `map` with its first character's top-left cell at (col, row), with
    /// the same return codes as ArcadeApi::drawText(). On any status but Ok, `map` is left
    /// unchanged (SR-FW-12).
    virtual ApiStatus layout(int col, int row, std::string_view text, TextScale scale,
                             Tilemap& map) const noexcept = 0;
};

}  // namespace arcade
