/// @file
/// The 5 x 7 font (SR-FW-12, section 3.1.1 Text).
#pragma once

#include <array>
#include <cstdint>

namespace arcade::assets {

/// First character with a font tile (section 3.1.1: ASCII 0x20-0x7E).
constexpr char kFontFirstChar = 0x20;
/// Last character with a font tile (section 3.1.1: ASCII 0x20-0x7E).
constexpr char kFontLastChar = 0x7E;

/// An 8 x 8 character cell: one byte per row, top row first, bit 7 = leftmost pixel.
using GlyphCell = std::array<std::uint8_t, 8>;

/// True if `ch` has a font tile (SR-FW-12).
constexpr bool hasGlyph(char ch) noexcept { return ch >= kFontFirstChar && ch <= kFontLastChar; }

/// Returns the character cell of `ch`, which must satisfy hasGlyph(). Scales 1 and 4 share this
/// cell, with the 5 x 7 glyph one pixel from the left and against the top (SR-FW-12).
GlyphCell glyphCell(char ch) noexcept;

}  // namespace arcade::assets
