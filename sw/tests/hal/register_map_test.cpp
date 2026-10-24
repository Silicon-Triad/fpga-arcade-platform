#include "hal/register_map.hpp"

#include <gtest/gtest.h>

namespace {
using namespace arcade::hal;

// Every value below is copied from SRS §3.1.4 (Table 3.3) directly, not
// from the header -- these catch a case where both frozen copies agree
// with each other but disagree with the SRS (see PR #4 review history).

// Bridge window (SR-HAL-01)
static_assert(BRIDGE_BASE == 0xFF200000u, "SR-HAL-01: bridge base must be 0xFF20_0000");
static_assert(BRIDGE_WINDOW_SIZE == 0x200000u, "SR-HAL-01: bridge window must be 2 MB");

// TBL-Tilemap / TBL-TilePattern / TBL-Palette (App. A)
static_assert(GRID_COLS == 128, "App. A: GRID_COLS must be 128");
static_assert(GRID_ROWS == 96, "App. A: GRID_ROWS must be 96");
static_assert(TILEMAP_BANK_SIZE == 12288, "App. A: TILEMAP_BANK_SIZE must be 12,288");
static_assert(TILE_PATTERN_COUNT == 256, "App. A: TILE_PATTERN_COUNT must be 256");
static_assert(TILE_PATTERN_RAM_SIZE == 8192, "App. A: TILE_PATTERN_RAM_SIZE must be 8,192");
static_assert(PALETTE_SIZE == 16, "App. A: PALETTE_SIZE must be 16");
static_assert(kLeftPixelNibbleShift == 0, "§3.1.4: low nibble = left pixel");
static_assert(kRightPixelNibbleShift == 4, "§3.1.4: high nibble = right pixel");

// TBL-RegisterMap offsets (§3.1.4, Table 3.3)
static_assert(kOffsetTilemapBank0 == 0x0000, "§3.1.4: Tilemap Bank 0 offset");
static_assert(kOffsetTilemapBank1 == 0x3000, "§3.1.4: Tilemap Bank 1 offset");
static_assert(kOffsetTilePatternRam == 0x6000, "§3.1.4: Tile Pattern RAM offset");
static_assert(kOffsetPaletteBase == 0x8000, "§3.1.4: Palette Base offset");
static_assert(kOffsetControlReg == 0x8040, "§3.1.4: Control Register offset");
static_assert(kOffsetStatusReg == 0x8044, "§3.1.4: Status Register offset");
static_assert(kOffsetFrameCounter == 0x8048, "§3.1.4: Frame Counter offset");
static_assert(kOffsetSwapReqFrontBank == 0x804C, "§3.1.4: Swap Request / Front Bank offset");
static_assert(kOffsetIdVersion == 0x8050, "§3.1.4: ID/Version offset");
static_assert(kOffsetToneFrequency == 0x8054, "§3.1.4: Tone Frequency offset");
static_assert(kOffsetToneDuration == 0x8058, "§3.1.4: Tone Duration offset");
static_assert(kOffsetAudioControl == 0x805C, "§3.1.4: Audio Control offset");

// TBL-RegisterMap reset values (§3.1.4, Table 3.3)
static_assert(kResetTilemapBank0 == 0x00, "§3.1.4: Tilemap Bank 0 reset");
static_assert(kResetTilemapBank1 == 0x00, "§3.1.4: Tilemap Bank 1 reset");
static_assert(kResetTilePatternRam == 0x00, "§3.1.4: Tile Pattern RAM reset");
static_assert(kResetPaletteEntry == 0x00000000, "§3.1.4: Palette entry reset");
static_assert(kResetControlReg == 0x00000000, "§3.1.4: Control Register reset");
static_assert(kResetStatusReg == 0x00000000,
              "§3.1.4: Status Register reset is 0 -- SRS does not set FPGA-ready at reset");
static_assert(kResetFrameCounter == 0x00000000, "§3.1.4: Frame Counter reset");
static_assert(kResetSwapReqFrontBank == 0x00000000, "§3.1.4: Swap Request / Front Bank reset");
static_assert(kResetIdVersion == 0xA2CADE01, "§3.1.4: ID/Version reset is the magic value");
static_assert(kResetToneFrequency == 0x0000, "§3.1.4: Tone Frequency reset");
static_assert(kResetToneDuration == 0x0000, "§3.1.4: Tone Duration reset");
static_assert(kResetAudioControl == 0x00000000, "§3.1.4: Audio Control reset");

// HAL_ID_MAGIC (SR-HAL-09)
static_assert(HAL_ID_MAGIC == 0xA2CADE01, "SR-HAL-09: HAL_ID_MAGIC must be 0xA2CADE01");
static_assert(HAL_ID_MAGIC == kResetIdVersion, "ID/Version reset must equal HAL_ID_MAGIC");

// Control Register bits (§3.1.4)
static_assert(kBitControlResetTilemap == 1, "§3.1.4: Control Register bit 1 = reset tilemap");
static_assert(kBitControlDisplayEnable == 2, "§3.1.4: Control Register bit 2 = display enable");

// Status Register bits (§3.1.4)
static_assert(kBitStatusVsync == 0, "§3.1.4: Status Register bit 0 = vsync flag");
static_assert(kBitStatusFpgaReady == 1, "§3.1.4: Status Register bit 1 = FPGA ready");
static_assert(kBitStatusSwapPending == 2, "§3.1.4: Status Register bit 2 = swap pending");

// Swap Request / Front Bank, bit 0 (§3.1.4)
static_assert(kBitSwapRequest == 0, "§3.1.4: Swap Request is bit 0 on write");
static_assert(kBitFrontBank == 0, "§3.1.4: Front Bank is bit 0 on read");

// Audio Control Register bits (§3.1.4)
static_assert(kBitAudioEnable == 0, "§3.1.4: Audio Control bit 0 = audio enable");
static_assert(kBitAudioMute == 1, "§3.1.4: Audio Control bit 1 = mute");

}  // namespace

TEST(RegisterMap, TBL_RegisterMap_OffsetsAndResets) {
    EXPECT_EQ(arcade::hal::BRIDGE_BASE, 0xFF200000u);
    EXPECT_EQ(arcade::hal::BRIDGE_WINDOW_SIZE, 0x200000u);
    EXPECT_EQ(arcade::hal::HAL_ID_MAGIC, 0xA2CADE01u);
    EXPECT_EQ(arcade::hal::kResetStatusReg, 0x00000000u);
}
