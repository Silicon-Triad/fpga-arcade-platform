#pragma once

#include <array>
#include <cstdint>

#include "hal/register_map.hpp"
#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"

namespace arcade::hal {

/// One tile pattern as stored in Tile Pattern RAM: 8x8 px, 4 bpp, 2 px per byte, low nibble =
/// left pixel (TBL-TilePattern, §3.1.4). Written as given; the HAL never repacks it.
using HalTilePattern = std::array<std::uint8_t, kBytesPerTilePattern>;
/// All TILE_PATTERN_COUNT patterns, in tile-index order (TBL-TilePattern, SR-REN-04).
using HalTileSet = std::array<HalTilePattern, TILE_PATTERN_COUNT>;
/// PALETTE_SIZE colours, each 0x00RRGGBB (TBL-Palette, §3.1.4, CON-06).
using HalPalette = std::array<std::uint32_t, PALETTE_SIZE>;

/// Start-up load and shutdown of the HAL (SR-HAL-13, SR-HAL-07). Works only through the
/// RegisterSpace seam; fixed-size, no heap use. TilemapHal (#35) is unchanged and separate.
class HalLifecycle {
  public:
    /// Bind to a register space that must outlive this object. No bridge access.
    explicit HalLifecycle(RegisterSpace& regs) noexcept;

    /// SR-HAL-13 (with SR-VGA-09): write all patterns to Tile Pattern RAM, then all palette
    /// entries to Palette Base + 4*i, then set Display Enable (Control bit 2) by a
    /// read-modify-write that keeps the other bits. Returns kInvalidPalette, writing nothing,
    /// if an entry has bits above 0x00RRGGBB, or kShutDown after shutdown(). On any error
    /// Display Enable is not written.
    HalStatus loadAssets(const HalTileSet& patterns, const HalPalette& palette) noexcept;

    /// SR-HAL-07: release the bridge (RegisterSpace::release(), the #34 munmap) without
    /// writing any register, so Display Enable stays set and the last presented frame
    /// ("SAFE TO SWITCH OFF", SR-FW-13) stays on screen. A second call does nothing.
    void shutdown() noexcept;

    /// True once shutdown() has run.
    bool isShutDown() const noexcept { return shutDown_; }

  private:
    static_assert(TILE_PATTERN_COUNT * kBytesPerTilePattern == TILE_PATTERN_RAM_SIZE,
                  "TBL-TilePattern: 256 patterns of 32 bytes fill the pattern RAM");

    RegisterSpace& regs_;
    bool shutDown_ = false;
};

}  // namespace arcade::hal
