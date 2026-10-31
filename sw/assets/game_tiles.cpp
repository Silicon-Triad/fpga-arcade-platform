#include "assets/game_tiles.hpp"

#include <algorithm>

#include "assets/palette.hpp"

namespace arcade::assets {
namespace {

constexpr int kBallCornerPx = 3;  // pixels cut from the outer corner to draw the ball round
constexpr int kBrickGapPx = 1;    // dark line under each brick and paddle tile

// TBL-TilePattern: two pixels per byte, low nibble = left pixel.
constexpr std::uint8_t packed(Colour colour) noexcept {
    const auto value = static_cast<unsigned>(colour);
    return static_cast<std::uint8_t>(value | (value << 4U));
}

TilePattern solid(Colour colour) noexcept {
    TilePattern tile{};
    tile.fill(packed(colour));
    return tile;
}

// A solid tile with its bottom pixel rows black, so stacked tiles read as separate bars.
TilePattern shaded(Colour colour) noexcept {
    TilePattern tile = solid(colour);
    const auto bytesPerRow = static_cast<std::size_t>(kTilePx / 2);
    std::fill(tile.end() - static_cast<std::ptrdiff_t>(kBrickGapPx * bytesPerRow), tile.end(),
              std::uint8_t{0});
    return tile;
}

// One quarter of the round ball; `right` and `bottom` say which outer corner to cut.
TilePattern ballQuarter(bool right, bool bottom) noexcept {
    TilePattern tile{};
    for (int y = 0; y < kTilePx; ++y) {
        for (int x = 0; x < kTilePx; ++x) {
            const int fromOuterX = right ? kTilePx - 1 - x : x;
            const int fromOuterY = bottom ? kTilePx - 1 - y : y;
            if (fromOuterX + fromOuterY < kBallCornerPx) {
                continue;
            }
            const auto index =
                static_cast<std::size_t>(y) * (kTilePx / 2) + static_cast<std::size_t>(x) / 2;
            const auto value = static_cast<unsigned>(Colour::White);
            tile[index] =
                static_cast<std::uint8_t>(tile[index] | (x % 2 == 0 ? value : value << 4U));
        }
    }
    return tile;
}

std::array<TilePattern, kGameTilesUsed> buildPatterns() noexcept {
    return {{
        // Snake (section 3.1.1 screen 3).
        solid(Colour::Grey),
        solid(Colour::DarkGreen),
        solid(Colour::Green),
        solid(Colour::Red),
        // Breakout, in BreakoutTile order (section 3.1.1 screen 4).
        solid(Colour::Grey),
        shaded(Colour::Red),
        shaded(Colour::Orange),
        shaded(Colour::Yellow),
        shaded(Colour::Green),
        shaded(Colour::Cyan),
        shaded(Colour::Blue),
        shaded(Colour::White),
        ballQuarter(false, false),
        ballQuarter(true, false),
        ballQuarter(false, true),
        ballQuarter(true, true),
        // Menu (section 3.1.1 screen 2).
        solid(Colour::DarkGrey),
        solid(Colour::Green),
    }};
}

}  // namespace

const std::array<TilePattern, kGameTilesUsed>& gameTilePatterns() noexcept {
    static const std::array<TilePattern, kGameTilesUsed> patterns = buildPatterns();
    return patterns;
}

}  // namespace arcade::assets
