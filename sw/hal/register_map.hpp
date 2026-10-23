// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
#pragma once

#include <cstddef>
#include <cstdint>

namespace arcade::hal {

// Bridge configuration (SRS §3.1.4, SR-HAL-01, App. A BRIDGE_WINDOW_SIZE)
constexpr std::uintptr_t kBridgeBaseAddress = 0xFF200000;
constexpr std::size_t kBridgeWindowSize = 0x200000;  // 2 MB

// Register Offsets (SRS §3.1.4, TBL-RegisterMap)
constexpr std::uint32_t kOffsetIdVersionReg = 0x0000;
constexpr std::uint32_t kOffsetControlReg = 0x0004;
constexpr std::uint32_t kOffsetStatusReg = 0x0008;
constexpr std::uint32_t kOffsetAudioControlReg = 0x000C;
constexpr std::uint32_t kOffsetAudioStatusReg = 0x0010;
constexpr std::uint32_t kOffsetAudioDataReg = 0x0014;
constexpr std::uint32_t kOffsetSwapReqFrontBankReg = 0x8000;
constexpr std::uint32_t kOffsetTilemapBaseReg = 0x8010;
constexpr std::uint32_t kOffsetTilePatternBaseReg = 0x8020;
constexpr std::uint32_t kOffsetPaletteBaseReg = 0x8030;
constexpr std::uint32_t kOffsetTimerReg = 0x8050;
constexpr std::uint32_t kOffsetIrqMaskReg = 0x805C;

// Reset Values (SRS §3.1.4, R-4.2.6)
constexpr std::uint32_t kResetIdVersionReg = 0xA2CADE01;
constexpr std::uint32_t kResetControlReg = 0x00000000;
constexpr std::uint32_t kResetStatusReg = 0x00000000;
constexpr std::uint32_t kResetAudioControlReg = 0x00000000;
constexpr std::uint32_t kResetAudioStatusReg = 0x00000000;
constexpr std::uint32_t kResetAudioDataReg = 0x00000000;
constexpr std::uint32_t kResetSwapReqFrontBankReg = 0x00000000;
constexpr std::uint32_t kResetTilemapBaseReg = 0x00000000;
constexpr std::uint32_t kResetTilePatternBaseReg = 0x00000000;
constexpr std::uint32_t kResetPaletteBaseReg = 0x00000000;
constexpr std::uint32_t kResetTimerReg = 0x00000000;
constexpr std::uint32_t kResetIrqMaskReg = 0x00000000;

// Magic Constant (HAL_ID_MAGIC)
constexpr std::uint32_t kHalIdMagic = 0xA2CADE01;

// Tilemap Constants (TBL-Tilemap)
constexpr std::size_t kGridCols = 128;
constexpr std::size_t kGridRows = 96;
constexpr std::size_t kTilemapBankSize = 12288;

// Tile Pattern Constants (TBL-TilePattern)
constexpr std::size_t kTilePatternCount = 256;
constexpr std::size_t kBytesPerTilePattern = 32;
constexpr std::size_t kTilePatternRamSize = 8192;

// Palette Constants (TBL-Palette)
constexpr std::size_t kPaletteSize = 16;
constexpr std::size_t kBytesPerPaletteEntry = 4;
constexpr std::uint32_t kPaletteRedShift = 16;
constexpr std::uint32_t kPaletteGreenShift = 8;
constexpr std::uint32_t kPaletteBlueShift = 0;
constexpr std::uint32_t kPaletteMask = 0x00FFFFFF;

// Pixel Packing & Nibble Order
constexpr std::size_t kPixelsPerByte = 2;
constexpr std::size_t kLeftPixelNibbleShift = 4;

// Control Bits
constexpr std::uint32_t kBitControlEnable = 0;
constexpr std::uint32_t kBitControlReset = 1;

// Status Bits
constexpr std::uint32_t kBitStatusFpgaReady = 0;
constexpr std::uint32_t kBitStatusVblank = 1;
constexpr std::uint32_t kBitStatusAudioFull = 2;

// Audio Control Bits
constexpr std::uint32_t kBitAudioControlEnable = 0;
constexpr std::uint32_t kBitAudioControlIrqEn = 1;

// Swap Request / Front Bank Bits (§3.1.4)
constexpr std::uint32_t kBitSwapRequest = 0;  // write 1 = Swap Request
constexpr std::uint32_t kBitFrontBank = 0;    // read = Front Bank

}  // namespace arcade::hal
