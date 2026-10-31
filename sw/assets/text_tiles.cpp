#include "assets/text_tiles.hpp"

#include <algorithm>
#include <cstddef>

#include "assets/font.hpp"
#include "assets/tile_ids.hpp"
#include "assets/tile_set.hpp"

namespace arcade::assets {
namespace {

constexpr int kFontPxPerQuad = 2;  // SR-FW-12: each mosaic tile shows 2 x 2 font pixels

TileIndex mosaicTileAt(const GlyphCell& cell, int tileCol, int tileRow) noexcept {
    const auto pixel = [&cell](int x, int y) {
        return (cell[static_cast<std::size_t>(y)] & (0x80U >> static_cast<unsigned>(x))) != 0U;
    };
    const int x = tileCol * kFontPxPerQuad;
    const int y = tileRow * kFontPxPerQuad;
    unsigned mask = 0;
    mask |= pixel(x, y) ? kQuadTopLeft : 0U;
    mask |= pixel(x + 1, y) ? kQuadTopRight : 0U;
    mask |= pixel(x, y + 1) ? kQuadBottomLeft : 0U;
    mask |= pixel(x + 1, y + 1) ? kQuadBottomRight : 0U;
    return static_cast<TileIndex>(kMosaicFirstTile + mask);
}

}  // namespace

ApiStatus textToTiles(int col, int row, std::string_view text, TextScale scale, CellSink sink,
                      void* context) noexcept {
    if (!std::all_of(text.begin(), text.end(), hasGlyph)) {
        return ApiStatus::InvalidArgument;
    }
    const int size = static_cast<int>(scale);  // SR-FW-12: 1 or 4 tiles per character
    if (col < 0 || row < 0 || text.size() > static_cast<std::size_t>(kGridCols) ||
        col + static_cast<int>(text.size()) * size > kGridCols || row + size > kGridRows) {
        return ApiStatus::OutOfRange;
    }
    int charCol = col;
    for (const char ch : text) {
        if (scale == TextScale::X1) {
            sink(context, charCol, row,
                 static_cast<TileIndex>(kFontFirstTile + (ch - kFontFirstChar)));
        } else {
            const GlyphCell cell = glyphCell(ch);
            for (int y = 0; y < size; ++y) {
                for (int x = 0; x < size; ++x) {
                    sink(context, charCol + x, row + y, mosaicTileAt(cell, x, y));
                }
            }
        }
        charCol += size;
    }
    return ApiStatus::Ok;
}

}  // namespace arcade::assets
