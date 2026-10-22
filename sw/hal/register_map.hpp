// FROZEN INTERFACE
#ifndef ARCADE_REGISTER_MAP_HPP
#define ARCADE_REGISTER_MAP_HPP

#include <cstdint>

namespace arcade::hal {

/// SRS §3.1.4, SR-HAL-01: Base physical address & window size of LW HPS-to-FPGA bridge
constexpr uintptr_t kBridgeBaseAddress = 0xFF200000;
constexpr size_t kBridgeWindowSize = 0x200000;  // 2 MB

/// SRS §3.1.4, TBL-RegisterMap: Register offsets within LW bridge window
constexpr uintptr_t kOffsetTilemapBank0 = 0x0000;    // 12,288 B (128x96)
constexpr uintptr_t kOffsetTilemapBank1 = 0x3000;    // 12,288 B (128x96)
constexpr uintptr_t kOffsetTilePatternRam = 0x6000;  // 8,192 B (256 tiles, 4bpp)
constexpr uintptr_t kOffsetPaletteBase = 0x8000;     // 64 B (16 entries x 32-bit)
constexpr uintptr_t kOffsetControlReg = 0x8040;      // 32-bit R/W
constexpr uintptr_t kOffsetStatusReg = 0x8044;       // 32-bit R
constexpr uintptr_t kOffsetFrameCounter = 0x8048;    // 32-bit R
constexpr uintptr_t kOffsetSwapReqFront = 0x804C;    // 1-bit W (SwapReq) / 1-bit R (FrontBank)
constexpr uintptr_t kOffsetIdVersion = 0x8050;       // 32-bit R
constexpr uintptr_t kOffsetToneFrequency = 0x8054;   // 16-bit R/W
constexpr uintptr_t kOffsetToneDuration = 0x8058;    // 16-bit R/W
constexpr uintptr_t kOffsetAudioControl = 0x805C;    // 32-bit R/W

/// SRS §3.1.4, SR-HAL-09: Expected magic ID value
constexpr uint32_t kHalIdMagic = 0xA2CADE01;

/// SRS §3.1.4, Control Register bits (0x8040)
constexpr uint32_t kControlBitResetTilemap = (1U << 1);
constexpr uint32_t kControlBitDisplayEnable = (1U << 2);

/// SRS §3.1.4, Status Register bits (0x8044)
constexpr uint32_t kStatusBitVsyncFlag = (1U << 0);
constexpr uint32_t kStatusBitFpgaReady = (1U << 1);
constexpr uint32_t kStatusBitSwapPending = (1U << 2);

/// SRS §3.1.4, Audio Control Register bits (0x805C)
constexpr uint32_t kAudioControlBitAudioEnable = (1U << 0);
constexpr uint32_t kAudioControlBitMute = (1U << 1);

}  // namespace arcade::hal

#endif  // ARCADE_REGISTER_MAP_HPP
