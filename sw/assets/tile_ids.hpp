/// @file
/// Tile numbering of the combined tile set (srs-huzaifa-rev3.md section 3.1.1, tile budget).
#pragma once

#include <cstddef>

#include "arcade/types.hpp"

namespace arcade::assets {

/// First font tile. Tile 0 is the space, so a tilemap cleared to 0 is blank (SR-REN-07).
constexpr TileIndex kFontFirstTile = 0;
/// Font tiles, ASCII 0x20-0x7E (section 3.1.1 tile budget).
constexpr std::size_t kFontTileCount = 95;
/// First quadrant-mosaic tile; mosaic tile n has quadrant mask n (SR-FW-12).
constexpr TileIndex kMosaicFirstTile = kFontFirstTile + kFontTileCount;
/// Quadrant-mosaic tiles (SR-FW-12, section 3.1.1 tile budget).
constexpr std::size_t kMosaicTileCount = 16;

/// Common layout tiles. They come out of the 145 tiles left for graphics (section 3.1.1).
constexpr TileIndex kTileFrame = kMosaicFirstTile + kMosaicTileCount;
constexpr TileIndex kTileAccent = kTileFrame + 1;   ///< Blue accent on rows 0 and 95.
constexpr TileIndex kTileBoxBlue = kTileFrame + 2;  ///< Paused and shutdown box frame.
constexpr TileIndex kTileBoxRed = kTileFrame + 3;   ///< Game-over and no-keyboard box frame.
constexpr TileIndex kTileDivider = kTileFrame + 4;  ///< Horizontal divider line.
/// First tile free for game graphics.
constexpr TileIndex kFirstGameTile = kTileFrame + 5;
/// Number of tiles free for game graphics.
constexpr std::size_t kGameTileCapacity = kTileCount - kFirstGameTile;

}  // namespace arcade::assets
