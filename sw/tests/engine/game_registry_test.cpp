// Unit tests for TBL-GameRegistry and the game tile allocation (issue #8, SR-FW-03, SR-FW-04).
#include "games/game_registry.hpp"

#include <gtest/gtest.h>

#include "assets/game_tiles.hpp"
#include "assets/tile_set.hpp"
#include "fake_engine_api.hpp"
#include "games/breakout/breakout.hpp"

namespace arcade {
namespace {

TEST(GameRegistry, SR_FW_04_RowsMatchTable) {
    EXPECT_EQ(kGameRegistry[0].game_id, kGameIdSnake);
    EXPECT_EQ(kGameRegistry[0].name, "SNAKE");
    EXPECT_EQ(kGameRegistry[0].menu_order, 0);
    EXPECT_EQ(kGameRegistry[1].game_id, kGameIdBreakout);
    EXPECT_EQ(kGameRegistry[1].name, "BREAKOUT");
    EXPECT_EQ(kGameRegistry[1].menu_order, 1);
}

// Each factory's game draws only blank cells and tiles of its own block.
TEST(GameRegistry, SR_FW_03_FactoriesCreateGamesThatDrawTheirOwnTiles) {
    for (std::size_t i = 0; i < kGameRegistry.size(); ++i) {
        auto game = kGameRegistry[i].factory();
        ASSERT_NE(game, nullptr);
        FakeEngineApi api;
        game->render(api);
        const assets::TileBlock& block = assets::kTileBlocks[i];
        bool drew = false;
        for (const auto& line : api.cells) {
            for (const TileIndex tile : line) {
                if (tile == assets::kFontFirstTile) {
                    continue;
                }
                drew = true;
                EXPECT_GE(tile, block.first);
                EXPECT_LT(tile, block.first + block.count);
            }
        }
        EXPECT_TRUE(drew);
    }
}

TEST(GameTiles, AllocationIsContiguousFromFirstGameTile) {
    std::size_t next = assets::kFirstGameTile;
    for (const assets::TileBlock& block : assets::kTileBlocks) {
        EXPECT_EQ(block.first, next);
        next += block.count;
    }
    EXPECT_EQ(next, assets::kFirstGameTile + assets::kGameTilesUsed);
    EXPECT_EQ(assets::kBreakoutGfxTileCount, kBreakoutTileCount);
}

TEST(GameTiles, EveryPatternDrawsAndFitsTheTileSet) {
    const auto& patterns = assets::gameTilePatterns();
    for (const TilePattern& pattern : patterns) {
        EXPECT_NE(pattern, TilePattern{});
    }
    const auto tiles = assets::buildTileSet(patterns.data(), patterns.size());
    ASSERT_TRUE(tiles.has_value());
    EXPECT_EQ(tiles.value_or(TileSet{})[assets::kTileSnakeFood], patterns[3]);
}

}  // namespace
}  // namespace arcade
