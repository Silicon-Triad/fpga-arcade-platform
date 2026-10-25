// Unit tests for the game loop and tick timing (issue #7, SR-FW-02, PR-03, PR-04).
#include "engine/game_loop.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "engine/tick_profiler.hpp"

namespace arcade {
namespace {

using CallLog = std::vector<std::string>;

/// Fake clock: time moves only when a test (or a fake) advances it.
class FakeClock final : public TickClock {
  public:
    std::uint64_t nowUs() noexcept override { return now; }
    std::uint64_t now = 0;
};

/// Fake backend: records calls and counts one displayed frame per present().
class FakeEngineApi final : public EngineApi {
  public:
    explicit FakeEngineApi(CallLog& log) : log_(log) {}

    ApiStatus setTile(int, int, TileIndex) noexcept override { return ApiStatus::Ok; }
    ApiStatus fillRect(int, int, int, int, TileIndex) noexcept override { return ApiStatus::Ok; }
    ApiStatus drawText(int, int, std::string_view, TextScale) noexcept override {
        return ApiStatus::Ok;
    }
    void clear(TileIndex) noexcept override {}
    ApiStatus present() noexcept override {
        log_.emplace_back("present");
        ++frames;
        if (clock != nullptr) {
            clock->now += presentUs;  // waiting for vsync is not tick work
        }
        return presentStatus;
    }
    InputFrame pollInput() noexcept override {
        log_.emplace_back("pollInput");
        return InputFrame{};
    }
    void playSound(EffectId) noexcept override {}
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return frames; }
    ApiStatus setInputContext(InputContext) noexcept override { return ApiStatus::Ok; }

    std::uint32_t frames = 1'000;  // not zero: the board counter runs from power-on
    ApiStatus presentStatus = ApiStatus::Ok;
    FakeClock* clock = nullptr;
    std::uint64_t presentUs = 0;

  private:
    CallLog& log_;
};

/// Fake screen: records calls; render() advances the fake clock by `renderUs`.
class FakeScreen final : public Screen {
  public:
    explicit FakeScreen(CallLog& log) : log_(log) {}
    void update(const InputFrame&) noexcept override { log_.emplace_back("update"); }
    void render(EngineApi&) noexcept override {
        log_.emplace_back("render");
        if (clock != nullptr) {
            clock->now += renderUs;
        }
    }
    FakeClock* clock = nullptr;
    std::uint64_t renderUs = 0;

  private:
    CallLog& log_;
};

TEST(GameLoop, SR_FW_02_TickOrderIsPollUpdateRenderPresent) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    FakeScreen screen(log);
    GameLoop loop(api, clock);
    loop.setScreen(&screen);

    constexpr int kTicks = 5;
    for (int i = 0; i < kTicks; ++i) {
        loop.runTick();
    }

    CallLog expected;
    for (int i = 0; i < kTicks; ++i) {
        expected.insert(expected.end(), {"pollInput", "update", "render", "present"});
    }
    EXPECT_EQ(log, expected);
}

TEST(GameLoop, SR_FW_02_OneTickPerFrame) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    FakeScreen screen(log);
    GameLoop loop(api, clock);
    loop.setScreen(&screen);

    constexpr std::uint64_t kTicks = std::uint64_t{2} * TICK_RATE;
    std::uint64_t n = 0;
    loop.run([&] { return n++ == kTicks; });

    const GameLoopReport r = loop.report();
    EXPECT_EQ(r.ticks, kTicks);
    EXPECT_EQ(loop.tickCount(), kTicks);
    EXPECT_EQ(r.frameCountStart, 1'000u);
    EXPECT_EQ(loop.startFrameCount(), 1'000u);
    // PR-03 check: tick count equals the change in frameCount().
    EXPECT_EQ(r.ticks, static_cast<std::uint64_t>(r.frameCountEnd - r.frameCountStart));
}

TEST(GameLoop, SR_FW_02_NoScreenStillPollsAndPresents) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    GameLoop loop(api, clock);
    loop.runTick();
    EXPECT_EQ(log, (CallLog{"pollInput", "present"}));
}

TEST(GameLoop, SR_FW_02_PresentErrorDoesNotThrow) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    FakeScreen screen(log);
    GameLoop loop(api, clock);
    loop.setScreen(&screen);

    api.presentStatus = ApiStatus::BackendError;
    loop.runTick();
    loop.runTick();
    api.presentStatus = ApiStatus::Ok;
    loop.runTick();

    EXPECT_EQ(loop.presentErrors(), 2u);
    EXPECT_EQ(loop.tickCount(), 3u);
    EXPECT_EQ(loop.report().presentErrors, 2u);
    EXPECT_EQ(log.size(), 12u);  // every tick ran in full
}

TEST(GameLoop, PR_04_LoopTimesTickWithInjectedClock) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    FakeScreen screen(log);
    screen.clock = &clock;
    api.clock = &clock;
    api.presentUs = 16'000;  // vsync wait, excluded from tick work
    GameLoop loop(api, clock, GameLoopConfig{true});
    loop.setScreen(&screen);

    screen.renderUs = 3'000;
    loop.runTick();
    screen.renderUs = 9'000;
    loop.runTick();

    const TickStats s = loop.report().tickStats;
    EXPECT_EQ(s.ticks, 2u);
    EXPECT_EQ(s.maxUs, 9'000u);
    EXPECT_EQ(s.overBudget, 1u);
}

TEST(GameLoop, PR_04_NoStatsWhenProfilingOff) {
    CallLog log;
    FakeClock clock;
    FakeEngineApi api(log);
    GameLoop loop(api, clock);
    loop.runTick();
    EXPECT_EQ(loop.report().tickStats.ticks, 0u);
}

TEST(TickProfiler, PR_04_ProfilerReportsPercentile) {
    TickProfiler p;
    // 1,000 ticks: 996 of 1 ms, one exactly at budget (not over), 3 over budget.
    for (int i = 0; i < 996; ++i) {
        p.record(1'000);
    }
    p.record(kTickBudgetUs);
    p.record(9'000);
    p.record(10'000);
    p.record(12'000);

    const TickStats s = p.stats();
    EXPECT_EQ(s.ticks, 1'000u);
    EXPECT_EQ(s.overBudget, 3u);
    EXPECT_EQ(s.maxUs, 12'000u);
    // Nearest rank: ceil(1000 * 0.999) = 999th smallest = 10,000 us.
    EXPECT_EQ(s.p999Us, 10'000u);
}

TEST(TickProfiler, PR_04_PercentileOfSmallSampleIsMax) {
    TickProfiler p;
    p.record(2'000);
    p.record(500);
    p.record(4'000);
    EXPECT_EQ(p.stats().p999Us, 4'000u);  // ceil(3 * 0.999) = 3rd smallest
}

TEST(TickProfiler, PR_04_EmptyAndPastCapacity) {
    TickProfiler empty;
    EXPECT_EQ(empty.stats().ticks, 0u);
    EXPECT_EQ(empty.stats().p999Us, 0u);

    TickProfiler p(2);
    p.record(1'000);
    p.record(1'000);
    p.record(20'000);  // past capacity: counted, not in the percentile
    const TickStats s = p.stats();
    EXPECT_EQ(s.ticks, 3u);
    EXPECT_EQ(s.overBudget, 1u);
    EXPECT_EQ(s.maxUs, 20'000u);
    EXPECT_EQ(s.p999Us, 1'000u);
}

TEST(MonotonicClock, PR_04_NeverGoesBackwards) {
    MonotonicClock c;
    const std::uint64_t a = c.nowUs();
    const std::uint64_t b = c.nowUs();
    EXPECT_GE(b, a);
    EXPECT_GT(a, 0u);
}

}  // namespace
}  // namespace arcade
