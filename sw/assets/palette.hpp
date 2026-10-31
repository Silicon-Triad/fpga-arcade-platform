/// @file
/// Colour palette (TBL-Palette).
#pragma once

#include <cstdint>

#include "arcade/types.hpp"

namespace arcade::assets {

/// Palette slots (TBL-Palette palette_index 0-15).
enum class Colour : std::uint8_t {
    Black = 0,  // section 3.1.1 screen 10b: palette entry 0 is black
    White,
    Red,
    Green,
    DarkGreen,
    Blue,
    Yellow,
    Orange,
    Cyan,
    Grey,
    DarkGrey,
};

/// DESIGN CHOICE, NOT AN SRS VALUE: the SRS names colours but gives no RGB rows for
/// TBL-Palette; only entry 0 = black is stated (section 3.1.1 screen 10b). Plain colours chosen
/// and approved by the area owner on issue #9. Packed as 0x00RRGGBB (TBL-Palette rgb).
constexpr Palette kPalette{
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x008000, 0x0000FF, 0xFFFF00, 0xFF8000,
    0x00FFFF, 0x808080, 0x404040, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
};

}  // namespace arcade::assets
