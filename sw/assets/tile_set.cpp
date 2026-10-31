#include "assets/tile_set.hpp"

#include <algorithm>

#include "assets/font.hpp"
#include "assets/palette.hpp"
#include "assets/tile_ids.hpp"

namespace arcade::assets {
namespace {

constexpr int kQuadPx = kTilePx / 2;  // SR-FW-12: one font pixel is 4 x 4 screen pixels
constexpr int kDividerTopPx = 3;      // divider line drawn on the two middle pixel rows
constexpr int kDividerBottomPx = 4;

// TBL-TilePattern: two pixels per byte, low nibble = left pixel.
void setPixel(TilePattern& tile, int x, int y, Colour colour) noexcept {
    const auto index =
        static_cast<std::size_t>(y) * (kTilePx / 2) + static_cast<std::size_t>(x) / 2;
    const auto value = static_cast<unsigned>(colour);
    const unsigned current = tile[index];
    tile[index] = static_cast<std::uint8_t>((x % 2 == 0) ? (current & 0xF0U) | value
                                                         : (current & 0x0FU) | (value << 4U));
}

TilePattern fontTile(char ch) noexcept {
    TilePattern tile{};
    const GlyphCell cell = glyphCell(ch);
    for (int y = 0; y < kTilePx; ++y) {
        for (int x = 0; x < kTilePx; ++x) {
            if ((cell[static_cast<std::size_t>(y)] & (0x80U >> static_cast<unsigned>(x))) != 0U) {
                setPixel(tile, x, y, Colour::White);
            }
        }
    }
    return tile;
}

TilePattern mosaicTile(unsigned mask) noexcept {
    TilePattern tile{};
    for (int y = 0; y < kTilePx; ++y) {
        for (int x = 0; x < kTilePx; ++x) {
            const unsigned quad = 1U << static_cast<unsigned>((y / kQuadPx) * 2 + x / kQuadPx);
            if ((mask & quad) != 0U) {
                setPixel(tile, x, y, Colour::White);
            }
        }
    }
    return tile;
}

TilePattern solidTile(Colour colour) noexcept {
    TilePattern tile{};
    for (int y = 0; y < kTilePx; ++y) {
        for (int x = 0; x < kTilePx; ++x) {
            setPixel(tile, x, y, colour);
        }
    }
    return tile;
}

TilePattern dividerTile() noexcept {
    TilePattern tile{};
    for (int x = 0; x < kTilePx; ++x) {
        setPixel(tile, x, kDividerTopPx, Colour::Grey);
        setPixel(tile, x, kDividerBottomPx, Colour::Grey);
    }
    return tile;
}

}  // namespace

std::optional<TileSet> buildTileSet(const TilePattern* gameTiles,
                                    std::size_t gameTileCount) noexcept {
    if (gameTileCount > kGameTileCapacity) {
        return std::nullopt;
    }
    TileSet tiles{};
    for (std::size_t i = 0; i < kFontTileCount; ++i) {
        tiles[kFontFirstTile + i] =
            fontTile(static_cast<char>(kFontFirstChar + static_cast<int>(i)));
    }
    for (std::size_t i = 0; i < kMosaicTileCount; ++i) {
        tiles[kMosaicFirstTile + i] = mosaicTile(static_cast<unsigned>(i));
    }
    tiles[kTileFrame] = solidTile(Colour::Grey);
    tiles[kTileAccent] = solidTile(Colour::Blue);
    tiles[kTileBoxBlue] = solidTile(Colour::Blue);
    tiles[kTileBoxRed] = solidTile(Colour::Red);
    tiles[kTileDivider] = dividerTile();
    if (gameTileCount > 0) {
        std::copy_n(gameTiles, gameTileCount, tiles.begin() + kFirstGameTile);
    }
    return tiles;
}

}  // namespace arcade::assets
