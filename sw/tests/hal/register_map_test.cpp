// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.

#include "hal/register_map.hpp"

#include <gtest/gtest.h>

static_assert(arcade::hal::kBridgeWindowSize == 0x200000,
              "Bridge window size must be 2 MB (SR-HAL-01)");
static_assert(arcade::hal::kResetStatusReg == 0x00000000, "Status register reset must be 0");
static_assert(arcade::hal::kHalIdMagic == 0xA2CADE01, "HAL_ID_MAGIC must match SRS §3.1.4");
static_assert(arcade::hal::kGridCols == 128, "GRID_COLS must be 128");
static_assert(arcade::hal::kGridRows == 96, "GRID_ROWS must be 96");

TEST(RegisterMap, TBL_RegisterMap_OffsetsAndResets) {
    EXPECT_EQ(arcade::hal::kBridgeBaseAddress, 0xFF200000u);
    EXPECT_EQ(arcade::hal::kBridgeWindowSize, 0x200000u);
    EXPECT_EQ(arcade::hal::kHalIdMagic, 0xA2CADE01u);
    EXPECT_EQ(arcade::hal::kResetStatusReg, 0x00000000u);
}
