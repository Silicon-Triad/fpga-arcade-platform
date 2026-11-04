#include "hal/hal_lifecycle.hpp"

#include <cstddef>

namespace arcade::hal {

namespace {

/// §3.1.4, CON-06: repack a colour as 0x00RRGGBB from its three 8-bit channels.
std::uint32_t packPaletteEntry(std::uint32_t rgb) noexcept {
    const std::uint32_t r = (rgb >> kPaletteRedShift) & kPaletteChannelMask;
    const std::uint32_t g = (rgb >> kPaletteGreenShift) & kPaletteChannelMask;
    const std::uint32_t b = (rgb >> kPaletteBlueShift) & kPaletteChannelMask;
    return (r << kPaletteRedShift) | (g << kPaletteGreenShift) | (b << kPaletteBlueShift);
}

}  // namespace

HalLifecycle::HalLifecycle(RegisterSpace& regs) noexcept : regs_(regs) {}

HalStatus HalLifecycle::loadAssets(const HalTileSet& patterns, const HalPalette& palette) noexcept {
    if (shutDown_) {
        return HalStatus::kShutDown;  // SR-HAL-07: no bridge access after shutdown
    }
    // Validate before any write, so a rejected load leaves the bridge untouched and
    // Display Enable clear (SR-HAL-13, SR-VGA-09).
    for (const std::uint32_t rgb : palette) {
        if (packPaletteEntry(rgb) != rgb) {
            return HalStatus::kInvalidPalette;
        }
    }
    // SR-HAL-13 step 1: tile patterns, as given, in tile-index order (TBL-TilePattern).
    for (std::size_t i = 0; i < TILE_PATTERN_COUNT; ++i) {
        const auto offset =
            kOffsetTilePatternRam + static_cast<std::uint32_t>(i * kBytesPerTilePattern);
        regs_.writeBlock8(offset, patterns[i].data(), kBytesPerTilePattern);
    }
    // SR-HAL-13 step 2: palette entries at Palette Base + 4*i (TBL-Palette, §3.1.4).
    for (std::size_t i = 0; i < PALETTE_SIZE; ++i) {
        const auto offset =
            kOffsetPaletteBase + static_cast<std::uint32_t>(i * kBytesPerPaletteEntry);
        regs_.write32(offset, packPaletteEntry(palette[i]));
    }
    // SR-HAL-13 step 3, only after both loads: set Display Enable, keeping other bits.
    const std::uint32_t control = regs_.read32(kOffsetControlReg);
    regs_.write32(kOffsetControlReg, control | (1u << kBitControlDisplayEnable));
    return HalStatus::kOk;
}

void HalLifecycle::shutdown() noexcept {
    if (shutDown_) {
        return;
    }
    shutDown_ = true;
    // SR-HAL-07: release only; no register write, so Display Enable stays set (SR-FW-13).
    regs_.release();
}

}  // namespace arcade::hal
