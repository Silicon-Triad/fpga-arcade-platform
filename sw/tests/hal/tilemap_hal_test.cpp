#include "hal/tilemap_hal.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using arcade::hal::test::Access;
using arcade::hal::test::FakeClock;
using arcade::hal::test::MockRegisterSpace;
using namespace std::chrono_literals;

constexpr std::uint32_t kSwapPendingMask = 1u << kBitStatusSwapPending;
constexpr int kCols = static_cast<int>(GRID_COLS);
constexpr int kRows = static_cast<int>(GRID_ROWS);

std::size_t countWrite8(const MockRegisterSpace& m, std::uint32_t lo, std::uint32_t hi) {
    std::size_t n = 0;
    for (const auto& a : m.log) {
        if (a.kind == Access::Kind::kWrite8 && a.offset >= lo && a.offset < hi) {
            ++n;
        }
    }
    return n;
}

std::uint32_t bank0(int col, int row) {
    return kOffsetTilemapBank0 + static_cast<std::uint32_t>(row * kCols + col);
}
std::uint32_t bank1(int col, int row) {
    return kOffsetTilemapBank1 + static_cast<std::uint32_t>(row * kCols + col);
}

struct HalFixture : ::testing::Test {
    MockRegisterSpace regs;
    FakeClock clock;
    TilemapHal hal{regs, clock};
};

TEST_F(HalFixture, SR_HAL_03_SetTileWritesShadowWithoutBridgeAccess) {
    EXPECT_EQ(hal.setTile(5, 7, 42), HalStatus::kOk);
    EXPECT_EQ(hal.setTile(kCols - 1, kRows - 1, 9), HalStatus::kOk);
    EXPECT_EQ(hal.shadowTile(5, 7), 42);
    EXPECT_EQ(hal.shadowTile(kCols - 1, kRows - 1), 9);
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(HalFixture, SR_HAL_03_SetTileOutOfRangeIsDistinctError) {
    EXPECT_EQ(hal.setTile(-1, 0, 1), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.setTile(0, -1, 1), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.setTile(kCols, 0, 1), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.setTile(0, kRows, 1), HalStatus::kOutOfRange);
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(HalFixture, SR_HAL_04_FillRectFullGridWithoutBridgeAccess) {
    EXPECT_EQ(hal.fillRect(0, 0, kCols, kRows, 7), HalStatus::kOk);
    for (int y = 0; y < kRows; ++y) {
        for (int x = 0; x < kCols; ++x) {
            ASSERT_EQ(hal.shadowTile(x, y), 7);
        }
    }
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(HalFixture, SR_HAL_04_FillRectPartialOnlyTouchesRect) {
    EXPECT_EQ(hal.fillRect(10, 20, 3, 2, 5), HalStatus::kOk);
    EXPECT_EQ(hal.shadowTile(10, 20), 5);
    EXPECT_EQ(hal.shadowTile(12, 21), 5);
    EXPECT_EQ(hal.shadowTile(13, 20), 0);
    EXPECT_EQ(hal.shadowTile(10, 22), 0);
    EXPECT_EQ(hal.shadowTile(9, 20), 0);
}

TEST_F(HalFixture, SR_HAL_04_FillRectOutOfRangeWritesNothing) {
    EXPECT_EQ(hal.fillRect(0, 0, kCols + 1, 1, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.fillRect(0, 0, 1, kRows + 1, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.fillRect(kCols - 1, 0, 2, 1, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.fillRect(-1, 0, 2, 1, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.fillRect(0, 0, -1, 1, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.fillRect(0, kRows, 0, 0, 3), HalStatus::kOutOfRange);
    EXPECT_EQ(hal.shadowTile(0, 0), 0);
    EXPECT_EQ(hal.shadowTile(kCols - 1, 0), 0);
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(HalFixture, SR_HAL_04_FillRectEmptyIsOkAndWritesNothing) {
    EXPECT_EQ(hal.fillRect(4, 4, 0, 5, 3), HalStatus::kOk);
    EXPECT_EQ(hal.fillRect(4, 4, 5, 0, 3), HalStatus::kOk);
    EXPECT_EQ(hal.shadowTile(4, 4), 0);
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(HalFixture, SR_HAL_08_OrderIsCellWritesThenSwapRequestThenPoll) {
    // Read 1 is the pending check before the copy (clear); reads 2 and 3 after the Swap
    // Request see pending; read 4 sees it cleared.
    regs.onStatusRead = [&](int n) { regs.status = (n == 2 || n == 3) ? kSwapPendingMask : 0; };
    hal.setTile(1, 1, 8);
    ASSERT_EQ(hal.present(), HalStatus::kOk);

    // First: the pending check, then the Front Bank read. Then only cell writes, one Swap
    // Request, then polls.
    ASSERT_GE(regs.log.size(), 2u);
    EXPECT_EQ(regs.log[0].kind, Access::Kind::kRead32);
    EXPECT_EQ(regs.log[0].offset, kOffsetStatusReg);
    EXPECT_EQ(regs.log[1].kind, Access::Kind::kRead32);
    EXPECT_EQ(regs.log[1].offset, kOffsetSwapReqFrontBank);
    regs.log.erase(regs.log.begin());
    enum : std::uint8_t { kCells, kSwap, kPoll } phase = kCells;
    std::size_t cells = 0;
    int swaps = 0;
    for (std::size_t i = 1; i < regs.log.size(); ++i) {
        const Access& a = regs.log[i];
        if (a.kind == Access::Kind::kWrite8) {
            ASSERT_EQ(phase, kCells) << "cell write after Swap Request at " << i;
            ++cells;
        } else if (a.kind == Access::Kind::kWrite32) {
            ASSERT_EQ(phase, kCells);
            EXPECT_EQ(a.offset, kOffsetSwapReqFrontBank);
            EXPECT_EQ(a.value, 1u << kBitSwapRequest);
            ++swaps;
            phase = kSwap;
        } else {
            ASSERT_EQ(a.kind, Access::Kind::kRead32);
            ASSERT_EQ(a.offset, kOffsetStatusReg);
            ASSERT_NE(phase, kCells) << "Status poll before Swap Request";
            phase = kPoll;
        }
    }
    EXPECT_EQ(cells, TILEMAP_BANK_SIZE);  // first present: whole back bank
    EXPECT_EQ(swaps, 1);
    EXPECT_EQ(regs.statusReads, 4);
}

TEST_F(HalFixture, SR_HAL_08_NoCellWriteWhileEarlierSwapStillPending) {
    // Hardware model for this test: a Swap Request sets pending and it never clears.
    regs.onStatusRead = [&](int) {
        clock.t += 1ms;
        for (const auto& a : regs.log) {
            if (a.kind == Access::Kind::kWrite32 && a.offset == kOffsetSwapReqFrontBank) {
                regs.status = kSwapPendingMask;
            }
        }
    };
    regs.front = 0;
    ASSERT_EQ(hal.present(), HalStatus::kSwapTimeout);  // bank 1 written, swap times out

    regs.log.clear();
    hal.setTile(2, 2, 9);
    EXPECT_EQ(hal.present(), HalStatus::kSwapTimeout);
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), 0u);
    for (const auto& a : regs.log) {
        EXPECT_NE(a.kind, Access::Kind::kWrite32) << "no second Swap Request while pending";
    }

    // The late swap lands: bank 1 is front, so the next present() writes bank 0 in full
    // (never written yet) and leaves bank 1 alone except the changed cell next time.
    regs.onStatusRead = nullptr;
    regs.status = 0;
    regs.front = 1;
    regs.log.clear();
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank0, kOffsetTilemapBank1), TILEMAP_BANK_SIZE);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank1, kOffsetTilePatternRam), 0u);
    EXPECT_EQ(regs.mem8[bank0(2, 2)], 9);

    regs.front = 0;
    regs.log.clear();
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank1, kOffsetTilePatternRam), 1u);
    EXPECT_EQ(regs.mem8[bank1(2, 2)], 9);
}

TEST_F(HalFixture, SR_HAL_08_WaitsForEarlierSwapThenCopies) {
    // Pending at entry, clears on the third Status read; the copy happens only after that.
    regs.onStatusRead = [&](int n) { regs.status = n < 3 ? kSwapPendingMask : 0; };
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    std::size_t firstWrite = regs.log.size();
    std::size_t lastPendingRead = 0;
    for (std::size_t i = 0; i < regs.log.size(); ++i) {
        if (regs.log[i].kind == Access::Kind::kWrite8 && firstWrite == regs.log.size()) {
            firstWrite = i;
        }
        if (regs.log[i].kind == Access::Kind::kRead32 && regs.log[i].offset == kOffsetStatusReg &&
            (regs.log[i].value & kSwapPendingMask) != 0u) {
            lastPendingRead = i;
        }
    }
    EXPECT_LT(lastPendingRead, firstWrite);
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), TILEMAP_BANK_SIZE);
}

TEST_F(HalFixture, SR_HAL_08_FirstPresentWritesWholeBackBank) {
    regs.front = 0;
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank1, kOffsetTilePatternRam), TILEMAP_BANK_SIZE);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank0, kOffsetTilemapBank1), 0u);

    regs.log.clear();
    regs.front = 1;
    ASSERT_EQ(hal.present(), HalStatus::kOk);  // bank 0 never written yet: whole bank
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank0, kOffsetTilemapBank1), TILEMAP_BANK_SIZE);
    EXPECT_EQ(countWrite8(regs, kOffsetTilemapBank1, kOffsetTilePatternRam), 0u);
}

TEST_F(HalFixture, SR_HAL_08_CopiesChangedCellsToTwoFrameOldBank) {
    // Bring both banks in sync with the shadow first.
    regs.front = 0;
    hal.present();
    regs.front = 1;
    hal.present();
    regs.log.clear();

    // Frame N: change one cell, present with front=0 -> writes bank 1 only that cell.
    hal.setTile(3, 4, 77);
    regs.front = 0;
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), 1u);
    EXPECT_EQ(regs.mem8[bank1(3, 4)], 77);
    regs.log.clear();

    // Frame N+1: no new change; bank 0 is two frames old and must still get the cell.
    regs.front = 1;
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), 1u);
    EXPECT_EQ(regs.mem8[bank0(3, 4)], 77);
    regs.log.clear();

    // Frame N+2: nothing changed since either bank was written; no cell writes.
    regs.front = 0;
    ASSERT_EQ(hal.present(), HalStatus::kOk);
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), 0u);
}

TEST_F(HalFixture, SR_HAL_08_SameValueWriteIsNotACopy) {
    regs.front = 0;
    hal.present();
    regs.front = 1;
    hal.present();
    regs.log.clear();
    hal.setTile(0, 0, 0);  // unchanged value
    regs.front = 0;
    hal.present();
    EXPECT_EQ(countWrite8(regs, 0, kOffsetTilePatternRam), 0u);
}

TEST_F(HalFixture, SR_HAL_08_BanksHoldShadowAfterEachPresent) {
    for (int frame = 0; frame < 6; ++frame) {
        hal.setTile(frame, frame, static_cast<std::uint8_t>(frame + 1));
        hal.fillRect(20, frame, 4, 1, static_cast<std::uint8_t>(100 + frame));
        regs.front = static_cast<std::uint32_t>(frame % 2);
        ASSERT_EQ(hal.present(), HalStatus::kOk);
        const std::uint32_t backBase = regs.front == 0 ? kOffsetTilemapBank1 : kOffsetTilemapBank0;
        for (int y = 0; y < kRows; ++y) {
            for (int x = 0; x < kCols; ++x) {
                ASSERT_EQ(regs.mem8[backBase + static_cast<std::uint32_t>(y * kCols + x)],
                          hal.shadowTile(x, y))
                    << "frame " << frame << " cell " << x << "," << y;
            }
        }
    }
}

TEST_F(HalFixture, SR_HAL_05_ReturnsAsSoonAsSwapPendingClears) {
    regs.status = kSwapPendingMask;
    regs.onStatusRead = [&](int n) {
        clock.t += 1ms;
        if (n == 5) {
            regs.status = 0;
        }
    };
    EXPECT_EQ(hal.waitSwapComplete(), HalStatus::kOk);
    EXPECT_EQ(regs.statusReads, 5);
}

TEST_F(HalFixture, SR_HAL_05_OtherStatusBitsDoNotHoldThePoll) {
    regs.status = ~kSwapPendingMask;
    EXPECT_EQ(hal.waitSwapComplete(), HalStatus::kOk);
    EXPECT_EQ(regs.statusReads, 1);
}

TEST_F(HalFixture, SR_HAL_05_TimesOutAfter100ms) {
    regs.status = kSwapPendingMask;  // never clears
    regs.onStatusRead = [&](int) { clock.t += 1ms; };
    EXPECT_EQ(hal.waitSwapComplete(), HalStatus::kSwapTimeout);
    // Polls at t = 1..101 ms; the first elapsed time beyond 100 ms ends the poll.
    EXPECT_EQ(regs.statusReads, 101);
    EXPECT_GT(clock.t, std::chrono::nanoseconds(VSYNC_TIMEOUT));
    EXPECT_LE(clock.t, std::chrono::nanoseconds(VSYNC_TIMEOUT + 1ms));
}

TEST_F(HalFixture, SR_HAL_05_NoEarlyTimeout) {
    regs.status = kSwapPendingMask;
    regs.onStatusRead = [&](int) {
        // Clears exactly at 100 ms: "within 100 ms" still counts as success.
        clock.t += 1ms;
        if (clock.t >= std::chrono::nanoseconds(VSYNC_TIMEOUT)) {
            regs.status = 0;
        }
    };
    EXPECT_EQ(hal.waitSwapComplete(), HalStatus::kOk);
    EXPECT_EQ(clock.t, std::chrono::nanoseconds(VSYNC_TIMEOUT));
}

TEST_F(HalFixture, SR_HAL_05_PresentReturnsTimeout) {
    regs.status = kSwapPendingMask;
    regs.onStatusRead = [&](int) { clock.t += 10ms; };
    EXPECT_EQ(hal.present(), HalStatus::kSwapTimeout);
}

TEST_F(HalFixture, SR_HAL_06_FrontBankAndFrameCounterReadsDoNotPoll) {
    regs.status = kSwapPendingMask;  // a wait would never end on its own
    regs.front = 1;
    regs.frameCounter = 0x12345678u;
    EXPECT_EQ(hal.frontBank(), 1u);
    EXPECT_EQ(hal.frameCounter(), 0x12345678u);
    regs.front = 0;
    EXPECT_EQ(hal.frontBank(), 0u);
    EXPECT_EQ(regs.statusReads, 0);
    ASSERT_EQ(regs.log.size(), 3u);  // exactly one register read per call
    EXPECT_EQ(regs.log[1].offset, kOffsetFrameCounter);
    EXPECT_EQ(clock.t, std::chrono::nanoseconds(0));
}

}  // namespace
