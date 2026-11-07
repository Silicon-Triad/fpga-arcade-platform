#include "hal/bridge_benchmark.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>

#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using arcade::hal::test::Access;
using arcade::hal::test::MockRegisterSpace;
using namespace std::chrono_literals;

/// Clock that returns `step` more on every now() call, so a run's elapsed time is known.
class SteppingClock final : public MonotonicClock {
  public:
    explicit SteppingClock(std::chrono::nanoseconds step) : step_(step) {}
    std::chrono::nanoseconds now() noexcept override {
        t_ += step_;
        return t_;
    }

  private:
    std::chrono::nanoseconds step_;
    std::chrono::nanoseconds t_{0};
};

TEST(BridgeBenchmark, Section314_TargetIs12288Times60) {
    EXPECT_EQ(kWorstCaseTilemapBytesPerSecond, 737280u);
}

TEST(BridgeBenchmark, Section314_WritesWholeBackBankOnlyPerPass) {
    for (std::uint32_t front : {0u, 1u}) {
        MockRegisterSpace m;
        m.front = front;
        SteppingClock clock(1ms);
        const auto r = benchmarkTilemapWrites(m, clock, 3);
        const std::uint32_t back = front == 0 ? kOffsetTilemapBank1 : kOffsetTilemapBank0;
        EXPECT_EQ(r.backBankOffset, back);
        EXPECT_EQ(r.bytesWritten, 3u * TILEMAP_BANK_SIZE);
        std::size_t writes = 0;
        for (const auto& a : m.log) {
            if (a.kind == Access::Kind::kWrite8) {
                ++writes;
                EXPECT_GE(a.offset, back);
                EXPECT_LT(a.offset, back + TILEMAP_BANK_SIZE);
            }
            EXPECT_NE(a.kind, Access::Kind::kWrite32);  // no swap request, no control write
        }
        EXPECT_EQ(writes, 3u * TILEMAP_BANK_SIZE);
    }
}

TEST(BridgeBenchmark, Section314_RateAndVerdict) {
    MockRegisterSpace m;
    // One pass, start and end each step 10 ms: 12,288 bytes in 10 ms = 1,228,800 B/s.
    SteppingClock fast(10ms);
    auto r = benchmarkTilemapWrites(m, fast, 1);
    EXPECT_EQ(r.elapsed, 10ms);
    EXPECT_EQ(r.bytesPerSecond, 1228800u);
    EXPECT_TRUE(r.meetsWorstCase);

    // 12,288 bytes in 1 s is far below 737,280 B/s.
    SteppingClock slow(1s);
    r = benchmarkTilemapWrites(m, slow, 1);
    EXPECT_EQ(r.bytesPerSecond, 12288u);
    EXPECT_FALSE(r.meetsWorstCase);
}

TEST(BridgeBenchmark, Section314_ExactlyAtTargetPasses) {
    MockRegisterSpace m;
    SteppingClock clock(std::chrono::nanoseconds(1000000000LL / 60));  // 1/60 s, inexact
    const auto r = benchmarkTilemapWrites(m, clock, 1);
    EXPECT_GE(r.bytesPerSecond, kWorstCaseTilemapBytesPerSecond);
    EXPECT_TRUE(r.meetsWorstCase);
}

TEST(BridgeBenchmark, Section314_ZeroPassesWritesNothing) {
    MockRegisterSpace m;
    SteppingClock clock(1ms);
    const auto r = benchmarkTilemapWrites(m, clock, 0);
    EXPECT_EQ(r.bytesWritten, 0u);
    EXPECT_EQ(r.bytesPerSecond, 0u);
    EXPECT_FALSE(r.meetsWorstCase);
    for (const auto& a : m.log) {
        EXPECT_NE(a.kind, Access::Kind::kWrite8);
    }
}

}  // namespace
