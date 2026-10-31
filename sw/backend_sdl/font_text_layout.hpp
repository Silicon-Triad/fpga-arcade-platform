/// @file
/// The real TextLayout: writes the shared font and mosaic tiles into a tilemap (SR-FW-12).
#pragma once

#include <cstddef>

#include "assets/text_tiles.hpp"
#include "backend_sdl/text_layout.hpp"

namespace arcade {

/// TextLayout over assets::textToTiles() (SR-FW-12).
class FontTextLayout : public TextLayout {
  public:
    /// Writes `text` into `map`; see TextLayout::layout() (SR-FW-12).
    ApiStatus layout(int col, int row, std::string_view text, TextScale scale,
                     Tilemap& map) const noexcept override {
        return assets::textToTiles(
            col, row, text, scale,
            [](void* context, int cellCol, int cellRow, TileIndex tile) noexcept {
                (*static_cast<Tilemap*>(context))[static_cast<std::size_t>(cellRow)]
                                                 [static_cast<std::size_t>(cellCol)] = tile;
            },
            &map);
    }
};

}  // namespace arcade
