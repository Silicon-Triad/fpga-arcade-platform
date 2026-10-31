/// @file
/// Allocation of the game graphics tiles after kFirstGameTile, and their patterns (section
/// 3.1.1 tile budget, TBL-TilePattern). Later games append a row and their patterns.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "arcade/tables.hpp"
#include "arcade/types.hpp"
#include "assets/tile_ids.hpp"

namespace arcade::assets {

/// One block of tiles owned by a game or by the menu.
struct TileBlock {
    std::uint8_t owner;  ///< game_id (TBL-GameRegistry), or kOwnerMenu.
    TileIndex first;     ///< First tile of the block.
    std::size_t count;   ///< Tiles in the block.
};

/// TileBlock::owner of the menu's own tiles; not a game_id.
constexpr std::uint8_t kOwnerMenu = 0xFF;
/// Snake tiles: wall, body, head, food (section 3.1.1 screen 3).
constexpr std::size_t kSnakeTileCount = 4;
/// Breakout tiles: wall, six brick colours, paddle, four ball quarters (section 3.1.1 screen 4).
constexpr std::size_t kBreakoutGfxTileCount = 12;
/// Menu tiles: highlight band; green for the bar and panel frame (section 3.1.1 screen 2).
constexpr std::size_t kMenuTileCount = 2;

/// The allocation table, contiguous from kFirstGameTile.
constexpr std::array<TileBlock, 3> kTileBlocks{{
    {kGameIdSnake, kFirstGameTile, kSnakeTileCount},
    {kGameIdBreakout, kFirstGameTile + kSnakeTileCount, kBreakoutGfxTileCount},
    {kOwnerMenu, kFirstGameTile + kSnakeTileCount + kBreakoutGfxTileCount, kMenuTileCount},
}};

/// Total tiles allocated by kTileBlocks.
constexpr std::size_t kGameTilesUsed = kSnakeTileCount + kBreakoutGfxTileCount + kMenuTileCount;
static_assert(kGameTilesUsed <= kGameTileCapacity, "section 3.1.1 tile budget");

constexpr TileIndex kTileSnakeWall = kTileBlocks[0].first;      ///< Snake wall.
constexpr TileIndex kTileSnakeBody = kTileSnakeWall + 1;        ///< Dark green (screen 3).
constexpr TileIndex kTileSnakeHead = kTileSnakeWall + 2;        ///< Bright green (screen 3).
constexpr TileIndex kTileSnakeFood = kTileSnakeWall + 3;        ///< Red (screen 3).
constexpr TileIndex kTileBreakoutFirst = kTileBlocks[1].first;  ///< BreakoutTile::Wall.
constexpr TileIndex kTileMenuBand = kTileBlocks[2].first;       ///< Selected item band.
constexpr TileIndex kTileMenuGreen = kTileMenuBand + 1;         ///< Green bar and panel frame.

/// Patterns of every allocated tile, in allocation order; pass to buildTileSet().
const std::array<TilePattern, kGameTilesUsed>& gameTilePatterns() noexcept;

}  // namespace arcade::assets
