// Unit tests for the font, mosaic and text layout (issue #9, SR-FW-12, section 3.1.1).
#include <gtest/gtest.h>

#include <map>
#include <set>
#include <string>
#include <utility>

#include "assets/font.hpp"
#include "assets/palette.hpp"
#include "assets/text_tiles.hpp"
#include "assets/tile_ids.hpp"
#include "assets/tile_set.hpp"

namespace arcade::assets {
namespace {

using Cells = std::map<std::pair<int, int>, TileIndex>;

void record(void* context, int col, int row, TileIndex tile) noexcept {
    (*static_cast<Cells*>(context))[{col, row}] = tile;
}

int pixelAt(const TilePattern& tile, int x, int y) {
    const unsigned byte = tile[static_cast<std::size_t>(y) * 4 + static_cast<std::size_t>(x) / 2];
    return static_cast<int>(x % 2 == 0 ? byte & 0x0FU : byte >> 4U);
}

TEST(Assets, SR_FW_12_BudgetIs95FontAnd16Mosaic) {
    EXPECT_EQ(kFontTileCount, 95U);
    EXPECT_EQ(kMosaicTileCount, 16U);
    EXPECT_EQ(kFontTileCount + kMosaicTileCount, 111U);
    EXPECT_EQ(kFontLastChar - kFontFirstChar + 1, 95);
}

TEST(Assets, SR_FW_12_EveryPrintableGlyphIsDistinctAndFits5x7) {
    std::set<GlyphCell> seen;
    for (int c = kFontFirstChar; c <= kFontLastChar; ++c) {
        const GlyphCell cell = glyphCell(static_cast<char>(c));
        seen.insert(cell);
        EXPECT_EQ(cell[7], 0) << c;  // seven rows
        for (const auto row : cell) {
            EXPECT_EQ(row & 0x83U, 0U) << c;  // five columns, one pixel from the left
        }
    }
    EXPECT_EQ(seen.size(), kFontTileCount);
}

TEST(Assets, SR_FW_12_LetterShapes) {
    constexpr GlyphCell kT{0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00};
    EXPECT_EQ(glyphCell('T'), kT);
}

TEST(Assets, SR_FW_12_MosaicTilesShowTheirQuadrants) {
    const TileSet tiles = buildTileSet(nullptr, 0).value_or(TileSet{});
    for (unsigned mask = 0; mask < kMosaicTileCount; ++mask) {
        const TilePattern& tile = tiles[kMosaicFirstTile + mask];
        const auto lit = [](bool on) { return on ? static_cast<int>(Colour::White) : 0; };
        EXPECT_EQ(pixelAt(tile, 0, 0), lit((mask & kQuadTopLeft) != 0));
        EXPECT_EQ(pixelAt(tile, 7, 0), lit((mask & kQuadTopRight) != 0));
        EXPECT_EQ(pixelAt(tile, 0, 7), lit((mask & kQuadBottomLeft) != 0));
        EXPECT_EQ(pixelAt(tile, 7, 7), lit((mask & kQuadBottomRight) != 0));
    }
}

TEST(Assets, TBL_TilePattern_LowNibbleIsLeftPixel) {
    const TileSet tiles = buildTileSet(nullptr, 0).value_or(TileSet{});
    // Mosaic mask 1 lights the top-left quadrant only: pixels 0-3 of row 0.
    const TilePattern& tile = tiles[kMosaicFirstTile + 1];
    EXPECT_EQ(tile[0], 0x11);
    EXPECT_EQ(tile[1], 0x11);
    EXPECT_EQ(tile[2], 0x00);
}

TEST(Assets, SR_REN_07_TileZeroIsBlank) {
    const TileSet tiles = buildTileSet(nullptr, 0).value_or(TileSet{});
    EXPECT_EQ(tiles[0], TilePattern{});
    EXPECT_EQ(kPalette[static_cast<std::size_t>(Colour::Black)], 0U);
}

TEST(Assets, GameTilesStartAtFirstGameTileAndAreBounded) {
    TilePattern game{};
    game.fill(0x22);
    const TileSet tiles = buildTileSet(&game, 1).value_or(TileSet{});
    EXPECT_EQ(tiles[kFirstGameTile], game);
    EXPECT_FALSE(buildTileSet(&game, kGameTileCapacity + 1).has_value());
}

TEST(TextTiles, SR_FW_12_Scale1UsesOneFontTilePerCharacter) {
    Cells cells;
    EXPECT_EQ(textToTiles(2, 1, "A ~", TextScale::X1, record, &cells), ApiStatus::Ok);
    ASSERT_EQ(cells.size(), 3U);
    EXPECT_EQ((cells[{2, 1}]), 'A' - 0x20);
    EXPECT_EQ((cells[{3, 1}]), 0);
    EXPECT_EQ((cells[{4, 1}]), 94);
}

TEST(TextTiles, SR_FW_12_Scale4Uses4x4MosaicTilesMatchingTheGlyph) {
    Cells cells;
    EXPECT_EQ(textToTiles(10, 20, "T", TextScale::X4, record, &cells), ApiStatus::Ok);
    ASSERT_EQ(cells.size(), 16U);
    const GlyphCell glyph = glyphCell('T');
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            const int tile = cells[{10 + x / 2, 20 + y / 2}] - kMosaicFirstTile;
            ASSERT_GE(tile, 0);
            ASSERT_LT(tile, 16);
            const auto mask = static_cast<unsigned>(tile);
            const auto bit = static_cast<unsigned>((y % 2) * 2 + (x % 2));
            const bool lit = ((mask >> bit) & 1U) != 0U;
            EXPECT_EQ(lit, (glyph[static_cast<std::size_t>(y)] &
                            (0x80U >> static_cast<unsigned>(x))) != 0U);
        }
    }
}

TEST(TextTiles, SR_FW_12_BadInputDrawsNothing) {
    Cells cells;
    EXPECT_EQ(textToTiles(0, 0, "A\n", TextScale::X1, record, &cells), ApiStatus::InvalidArgument);
    EXPECT_EQ(textToTiles(0, 0, "\x7F", TextScale::X4, record, &cells), ApiStatus::InvalidArgument);
    EXPECT_EQ(textToTiles(-1, 0, "A", TextScale::X1, record, &cells), ApiStatus::OutOfRange);
    EXPECT_EQ(textToTiles(0, -1, "A", TextScale::X1, record, &cells), ApiStatus::OutOfRange);
    EXPECT_EQ(textToTiles(127, 0, "AB", TextScale::X1, record, &cells), ApiStatus::OutOfRange);
    EXPECT_EQ(textToTiles(125, 0, "A", TextScale::X4, record, &cells), ApiStatus::OutOfRange);
    EXPECT_EQ(textToTiles(0, 93, "A", TextScale::X4, record, &cells), ApiStatus::OutOfRange);
    EXPECT_EQ(textToTiles(0, 0, std::string(200, 'A'), TextScale::X1, record, &cells),
              ApiStatus::OutOfRange);
    EXPECT_TRUE(cells.empty());
}

TEST(TextTiles, SR_FW_12_TextFitsExactlyToTheGridEdge) {
    Cells cells;
    EXPECT_EQ(textToTiles(124, 92, "A", TextScale::X4, record, &cells), ApiStatus::Ok);
    EXPECT_EQ(cells.size(), 16U);
}

}  // namespace
}  // namespace arcade::assets
