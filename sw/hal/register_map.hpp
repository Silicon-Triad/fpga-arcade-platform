// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
#pragma once

#include <cstddef>
#include <cstdint>

namespace arcade::hal {

// ---------------------------------------------------------------------------
// Bridge window (SRS §3.1.4, SR-HAL-01; App. A BRIDGE_BASE, BRIDGE_WINDOW_SIZE)
// ---------------------------------------------------------------------------
inline constexpr std::uintptr_t BRIDGE_BASE = 0xFF200000;    // §3.1.4, SR-HAL-01
inline constexpr std::size_t BRIDGE_WINDOW_SIZE = 0x200000;  // §3.1.4, SR-HAL-01, 2 MB

// ---------------------------------------------------------------------------
// TBL-Tilemap (App. A GRID_COLS / GRID_ROWS / TILEMAP_BANK_SIZE)
// ---------------------------------------------------------------------------
inline constexpr std::size_t GRID_COLS = 128;            // SR-FW-12, UR-06
inline constexpr std::size_t GRID_ROWS = 96;             // SR-FW-12, UR-06
inline constexpr std::size_t TILEMAP_BANK_SIZE = 12288;  // SR-REN-01, SR-HAL-03/04

// ---------------------------------------------------------------------------
// TBL-TilePattern (App. A TILE_PATTERN_COUNT / TILE_PATTERN_RAM_SIZE)
// ---------------------------------------------------------------------------
inline constexpr std::size_t TILE_PATTERN_COUNT = 256;      // SR-REN-04
inline constexpr std::size_t TILE_PATTERN_RAM_SIZE = 8192;  // SR-REN-04
inline constexpr std::size_t kBytesPerTilePattern = 32;     // §3.1.4: 8x8 px, 4bpp, 2 px/byte

// Pixel packing within a tile-pattern byte (§3.1.4, TBL-TilePattern):
// "Packed 2 pixels/byte; low nibble = left pixel, high nibble = right pixel."
inline constexpr unsigned kPixelsPerByte = 2;          // §3.1.4
inline constexpr unsigned kLeftPixelNibbleShift = 0;   // §3.1.4: low nibble = left pixel
inline constexpr unsigned kRightPixelNibbleShift = 4;  // §3.1.4: high nibble = right pixel

// ---------------------------------------------------------------------------
// TBL-Palette (App. A PALETTE_SIZE)
// ---------------------------------------------------------------------------
inline constexpr std::size_t PALETTE_SIZE = 16;          // SR-REN-05
inline constexpr std::size_t kBytesPerPaletteEntry = 4;  // §3.1.4: 32 bits, Palette Base

// Palette entry packing: 0x00RRGGBB, 8 bits/channel (§3.1.4, CON-06)
inline constexpr unsigned kPaletteRedShift = 16;                   // §3.1.4, CON-06
inline constexpr unsigned kPaletteGreenShift = 8;                  // §3.1.4, CON-06
inline constexpr unsigned kPaletteBlueShift = 0;                   // §3.1.4, CON-06
inline constexpr std::uint32_t kPaletteChannelMask = 0x000000FFu;  // §3.1.4, CON-06

// ---------------------------------------------------------------------------
// TBL-RegisterMap (§3.1.4, Table 3.3) -- all 12 offsets, in table order
// ---------------------------------------------------------------------------
inline constexpr std::uint32_t kOffsetTilemapBank0 = 0x0000;  // §3.1.4
inline constexpr std::uint8_t kResetTilemapBank0 = 0x00;      // §3.1.4

inline constexpr std::uint32_t kOffsetTilemapBank1 = 0x3000;  // §3.1.4
inline constexpr std::uint8_t kResetTilemapBank1 = 0x00;      // §3.1.4

inline constexpr std::uint32_t kOffsetTilePatternRam = 0x6000;  // §3.1.4
inline constexpr std::uint8_t kResetTilePatternRam = 0x00;      // §3.1.4

inline constexpr std::uint32_t kOffsetPaletteBase = 0x8000;      // §3.1.4
inline constexpr std::uint32_t kResetPaletteEntry = 0x00000000;  // §3.1.4

inline constexpr std::uint32_t kOffsetControlReg = 0x8040;     // §3.1.4
inline constexpr std::uint32_t kResetControlReg = 0x00000000;  // §3.1.4

inline constexpr std::uint32_t kOffsetStatusReg = 0x8044;     // §3.1.4
inline constexpr std::uint32_t kResetStatusReg = 0x00000000;  // §3.1.4 (NOT FPGA-ready-set)

inline constexpr std::uint32_t kOffsetFrameCounter = 0x8048;     // §3.1.4
inline constexpr std::uint32_t kResetFrameCounter = 0x00000000;  // §3.1.4

inline constexpr std::uint32_t kOffsetSwapReqFrontBank = 0x804C;     // §3.1.4
inline constexpr std::uint32_t kResetSwapReqFrontBank = 0x00000000;  // §3.1.4

inline constexpr std::uint32_t kOffsetIdVersion = 0x8050;     // §3.1.4
inline constexpr std::uint32_t kResetIdVersion = 0xA2CADE01;  // §3.1.4

inline constexpr std::uint32_t kOffsetToneFrequency = 0x8054;  // §3.1.4, SR-AUD-04
inline constexpr std::uint16_t kResetToneFrequency = 0x0000;   // §3.1.4

inline constexpr std::uint32_t kOffsetToneDuration = 0x8058;  // §3.1.4, SR-AUD-05
inline constexpr std::uint16_t kResetToneDuration = 0x0000;   // §3.1.4

inline constexpr std::uint32_t kOffsetAudioControl = 0x805C;     // §3.1.4
inline constexpr std::uint32_t kResetAudioControl = 0x00000000;  // §3.1.4

// ---------------------------------------------------------------------------
// HAL_ID_MAGIC (App. A; SR-HAL-09) -- same value as kResetIdVersion above
// ---------------------------------------------------------------------------
inline constexpr std::uint32_t HAL_ID_MAGIC = 0xA2CADE01;

// ---------------------------------------------------------------------------
// Control Register bits (§3.1.4: bit0 reserved, bit1 reset tilemap, bit2 display enable)
// Build masks at point of use as (1u << kBitXxx); no separate mask constants
// here, so the two frozen files stay bit-for-bit comparable (position only).
// ---------------------------------------------------------------------------
inline constexpr unsigned kBitControlResetTilemap = 1;   // §3.1.4
inline constexpr unsigned kBitControlDisplayEnable = 2;  // §3.1.4, SR-HAL-13, SR-VGA-09

// ---------------------------------------------------------------------------
// Status Register bits (§3.1.4: bit0 vsync, bit1 FPGA ready, bit2 swap pending)
// ---------------------------------------------------------------------------
inline constexpr unsigned kBitStatusVsync = 0;        // §3.1.4
inline constexpr unsigned kBitStatusFpgaReady = 1;    // §3.1.4
inline constexpr unsigned kBitStatusSwapPending = 2;  // §3.1.4, SR-HAL-05, SR-REN-03

// ---------------------------------------------------------------------------
// Swap Request / Front Bank, bit 0 (§3.1.4): write 1 = Swap Request,
// read = Front Bank. Same bit position, two meanings -- named separately
// so a read (SR-HAL-06) and a write (SR-HAL-08) are never confused.
// ---------------------------------------------------------------------------
inline constexpr unsigned kBitSwapRequest = 0;  // §3.1.4, write side
inline constexpr unsigned kBitFrontBank = 0;    // §3.1.4, read side

// ---------------------------------------------------------------------------
// Audio Control Register bits (§3.1.4: bit0 audio enable, bit1 mute)
// ---------------------------------------------------------------------------
inline constexpr unsigned kBitAudioEnable = 0;  // §3.1.4, SR-HAL-10
inline constexpr unsigned kBitAudioMute = 1;    // §3.1.4, SR-HAL-12

}  // namespace arcade::hal
