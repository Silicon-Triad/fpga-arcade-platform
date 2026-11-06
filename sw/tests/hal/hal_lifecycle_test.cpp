#include "hal/hal_lifecycle.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>

#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using arcade::hal::test::Access;
using arcade::hal::test::MockRegisterSpace;

constexpr std::uint32_t kDisplayEnableMask = 1u << kBitControlDisplayEnable;
constexpr std::uint32_t kResetTilemapMask = 1u << kBitControlResetTilemap;
constexpr std::uint32_t kPaletteEnd =
    kOffsetPaletteBase + static_cast<std::uint32_t>(PALETTE_SIZE * kBytesPerPaletteEntry);

HalTileSet makePatterns() {
    HalTileSet set{};
    for (std::size_t t = 0; t < set.size(); ++t) {
        for (std::size_t b = 0; b < set[t].size(); ++b) {
            set[t][b] = static_cast<std::uint8_t>(t * 7 + b * 13 + 1);  // distinct-ish bytes
        }
    }
    return set;
}

HalPalette makePalette() {
    HalPalette p{};
    for (std::size_t i = 0; i < p.size(); ++i) {
        p[i] = static_cast<std::uint32_t>(0x00102030u + i * 0x00010101u);
    }
    return p;
}

bool isControlWrite(const Access& a) {
    return a.kind == Access::Kind::kWrite32 && a.offset == kOffsetControlReg;
}

struct LifecycleFixture : ::testing::Test {
    MockRegisterSpace regs;
    HalLifecycle hal{regs};
    HalTileSet patterns = makePatterns();
    HalPalette palette = makePalette();
};

TEST_F(LifecycleFixture, SR_HAL_13_DisplayEnableOnlyAfterPatternsAndPalette) {
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    std::size_t patternWrites = 0;
    std::size_t paletteWrites = 0;
    std::size_t controlWrites = 0;
    for (const auto& a : regs.log) {
        if (isControlWrite(a)) {
            ++controlWrites;
            EXPECT_EQ(patternWrites, TILE_PATTERN_RAM_SIZE);
            EXPECT_EQ(paletteWrites, PALETTE_SIZE);
        } else if (a.kind == Access::Kind::kWrite8 && a.offset >= kOffsetTilePatternRam &&
                   a.offset < kOffsetTilePatternRam + TILE_PATTERN_RAM_SIZE) {
            EXPECT_EQ(controlWrites, 0u);
            ++patternWrites;
        } else if (a.kind == Access::Kind::kWrite32 && a.offset >= kOffsetPaletteBase &&
                   a.offset < kPaletteEnd) {
            EXPECT_EQ(controlWrites, 0u);
            EXPECT_EQ(patternWrites, TILE_PATTERN_RAM_SIZE);  // patterns before palette
            ++paletteWrites;
        }
    }
    EXPECT_EQ(controlWrites, 1u);
    EXPECT_TRUE(isControlWrite(regs.log.back()));
    EXPECT_NE(regs.control & kDisplayEnableMask, 0u);
}

TEST_F(LifecycleFixture, SR_HAL_13_PatternBytesLandUnchangedAtTheirOffsets) {
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    std::size_t n = 0;
    for (const auto& a : regs.log) {
        if (a.kind != Access::Kind::kWrite8) {
            continue;
        }
        const std::size_t rel = a.offset - kOffsetTilePatternRam;
        ASSERT_LT(rel, TILE_PATTERN_RAM_SIZE);
        EXPECT_EQ(a.value, patterns[rel / kBytesPerTilePattern][rel % kBytesPerTilePattern]);
        ++n;
    }
    EXPECT_EQ(n, TILE_PATTERN_RAM_SIZE);
}

TEST_F(LifecycleFixture, SR_HAL_13_PaletteWrittenAs00RRGGBBAtBasePlus4i) {
    palette[3] = 0x00FF8001u;  // R=FF, G=80, B=01
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    std::size_t i = 0;
    for (const auto& a : regs.log) {
        if (a.kind == Access::Kind::kWrite32 && a.offset >= kOffsetPaletteBase &&
            a.offset < kPaletteEnd) {
            EXPECT_EQ(a.offset, kOffsetPaletteBase + 4u * i);
            EXPECT_EQ(a.value, palette[i]);
            ++i;
        }
    }
    EXPECT_EQ(i, PALETTE_SIZE);
}

TEST_F(LifecycleFixture, SR_HAL_13_DisplayEnableWrittenAloneWithoutReadBack) {
    regs.control = 0xF0000001u | kResetTilemapMask;  // stale bits from an earlier run
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    std::size_t controlAccesses = 0;
    for (const auto& a : regs.log) {
        if (a.offset == kOffsetControlReg) {
            ++controlAccesses;
            EXPECT_TRUE(isControlWrite(a));  // never read
        }
    }
    EXPECT_EQ(controlAccesses, 1u);
    EXPECT_EQ(regs.log.back().value, kDisplayEnableMask);  // bit 1 and reserved bits are 0
}

TEST_F(LifecycleFixture, SR_HAL_13_RejectedPaletteLeavesDisplayEnableClear) {
    palette[15] = 0x01000000u;  // bits above 0x00RRGGBB
    EXPECT_EQ(hal.loadAssets(patterns, palette), HalStatus::kInvalidPalette);
    EXPECT_TRUE(regs.log.empty());
    EXPECT_EQ(regs.control & kDisplayEnableMask, 0u);
}

TEST_F(LifecycleFixture, SR_HAL_07_LoadAfterShutdownIsRefused) {
    hal.shutdown();
    EXPECT_EQ(hal.loadAssets(patterns, palette), HalStatus::kShutDown);
    EXPECT_TRUE(regs.log.empty());
    EXPECT_EQ(regs.accessesAfterRelease, 0);
}

TEST_F(LifecycleFixture, SR_HAL_07_ShutdownKeepsDisplayEnableAndTouchesNoRegion) {
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    const std::size_t before = regs.log.size();
    hal.shutdown();
    EXPECT_EQ(regs.log.size(), before);  // no tilemap, pattern, palette or control access
    EXPECT_NE(regs.control & kDisplayEnableMask, 0u);
}

TEST_F(LifecycleFixture, SR_HAL_07_ShutdownReleasesOnceAndIsSafeTwice) {
    ASSERT_EQ(hal.loadAssets(patterns, palette), HalStatus::kOk);
    hal.shutdown();
    hal.shutdown();
    EXPECT_TRUE(hal.isShutDown());
    EXPECT_EQ(regs.releaseCalls, 1);
    EXPECT_EQ(regs.accessesAfterRelease, 0);
}

}  // namespace
