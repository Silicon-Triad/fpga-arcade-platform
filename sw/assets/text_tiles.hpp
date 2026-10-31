/// @file
/// Backend-neutral text layout: turns a string into tilemap cells (SR-FW-12).
#pragma once

#include <string_view>

#include "arcade/types.hpp"

namespace arcade::assets {

/// Receives one cell; `context` is the pointer given to textToTiles().
using CellSink = void (*)(void* context, int col, int row, TileIndex tile) noexcept;

/// Sends the tiles of `text` to `sink`, first character's top-left cell at (col, row).
/// Scale X1 gives one font tile per character; X4 gives 4 x 4 mosaic tiles (SR-FW-12).
/// @return Ok; InvalidArgument for a character with no font tile, a scale other than X1 or X4,
/// or a null `sink`; OutOfRange if the text does not fit the grid. On any status but Ok, `sink` is
/// never called (SR-FW-12).
ApiStatus textToTiles(int col, int row, std::string_view text, TextScale scale, CellSink sink,
                      void* context) noexcept;

}  // namespace arcade::assets
