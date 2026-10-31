/// @file
/// Builds the combined tile set loaded once at start-up (ArcadeApi::loadTiles, SR-FW-12).
#pragma once

#include <cstddef>
#include <optional>

#include "arcade/types.hpp"

namespace arcade::assets {

/// Mosaic quadrant bits of a tile index offset (SR-FW-12: each tile shows 2 x 2 font pixels).
constexpr unsigned kQuadTopLeft = 1U;
constexpr unsigned kQuadTopRight = 2U;
constexpr unsigned kQuadBottomLeft = 4U;
constexpr unsigned kQuadBottomRight = 8U;

/// Returns the font, mosaic and common tiles followed by `gameTileCount` game tiles, which start
/// at kFirstGameTile (section 3.1.1 tile budget, TBL-TilePattern).
/// @return nullopt if the game tiles exceed kGameTileCapacity.
std::optional<TileSet> buildTileSet(const TilePattern* gameTiles,
                                    std::size_t gameTileCount) noexcept;

}  // namespace arcade::assets
