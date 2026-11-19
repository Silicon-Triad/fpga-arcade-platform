#include "hal/frame_rate.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using namespace arcade::hal::fps;
using arcade::hal::test::FakeClock;
using arcade::hal::test::MockRegisterSpace;

/// A fake board: sleeping advances the clock and the Frame Counter at `hz` frames per second,
/// with the counter starting at `start`. Frames are whole counts, as on the hardware.
struct FakeBoard {
    MockRegisterSpace regs;
    FakeClock clock;
    double hz = 60.0;
    std::uint32_t start = 0;
    bool stallAfterFirst = false;
    int sleeps = 0;

    void sleep(std::chrono::nanoseconds d) {
        ++sleeps;
        clock.t += d;
        if (stallAfterFirst && sleeps > 1) {
            return;  // counter frozen from the second interval on
        }
        const double s = static_cast<double>(clock.t.count()) / 1e9;
        regs.frameCounter = start + static_cast<std::uint32_t>(static_cast<std::uint64_t>(s * hz));
    }

    Result run(const Options& opt, std::vector<Sample>* seen = nullptr) {
        regs.frameCounter = start;
        return measure(
            regs, clock, [this](std::chrono::nanoseconds d) { sleep(d); }, opt,
            [seen](const Sample& s) {
                if (seen != nullptr) {
                    seen->push_back(s);
                }
            });
    }
};

Options opts(std::uint32_t seconds, std::uint32_t sample) {
    Options o;
    o.seconds = seconds;
    o.sample = sample;
    return o;
}

}  // namespace

// PR-01: 60 Hz +/- 0.5 % = 59.7 Hz to 60.3 Hz, limits included.
TEST(FrameRate, PR_01_LimitsAndEdges) {
    EXPECT_DOUBLE_EQ(kPr01MinHz, 59.7);
    EXPECT_DOUBLE_EQ(kPr01MaxHz, 60.3);
    EXPECT_TRUE(meetsPr01(60.0));
    EXPECT_TRUE(meetsPr01(59.7));
    EXPECT_TRUE(meetsPr01(60.3));
    EXPECT_FALSE(meetsPr01(59.69));
    EXPECT_FALSE(meetsPr01(60.31));
    EXPECT_FALSE(meetsPr01(0.0));
}

// One count either side of each limit over 600 s: 35,820 and 36,180 frames are the limits.
TEST(FrameRate, PR_01_OneCountAtTheLimitsOver600s) {
    const std::uint64_t ns = 600ull * 1000000000ull;
    EXPECT_TRUE(meetsPr01(measuredHz(35820, ns)));
    EXPECT_FALSE(meetsPr01(measuredHz(35819, ns)));
    EXPECT_TRUE(meetsPr01(measuredHz(36180, ns)));
    EXPECT_FALSE(meetsPr01(measuredHz(36181, ns)));
}

TEST(FrameRate, MeasuredHzAndZeroTime) {
    EXPECT_DOUBLE_EQ(measuredHz(600, 10000000000ull), 60.0);
    EXPECT_DOUBLE_EQ(measuredHz(600, 0), 0.0);  // no division by zero
}

// §3.1.4 Frame Counter is 32-bit: a wrap from 0xFFFFFFF0 to 0x10 is 0x20 frames.
TEST(FrameRate, CounterWrap) {
    EXPECT_EQ(countDelta(0xFFFFFFF0u, 0x10u), 0x20u);
    EXPECT_EQ(countDelta(100u, 700u), 600u);
    EXPECT_EQ(countDelta(5u, 5u), 0u);
}

TEST(FrameRate, MeasureAt60HzPasses) {
    FakeBoard b;
    std::vector<Sample> seen;
    const Result r = b.run(opts(600, 10), &seen);
    EXPECT_EQ(r.frames, 36000u);
    EXPECT_EQ(r.ns, 600ull * 1000000000ull);
    EXPECT_DOUBLE_EQ(r.hz, 60.0);
    EXPECT_FALSE(r.stalled);
    EXPECT_TRUE(r.passed());
    ASSERT_EQ(seen.size(), 60u);  // one sample every 10 s
    EXPECT_EQ(seen.front().interval, 600u);
    EXPECT_DOUBLE_EQ(seen.front().intervalHz, 60.0);
}

// The run crosses the 32-bit wrap; the total must still be 36,000 frames.
TEST(FrameRate, MeasureAcrossWrap) {
    FakeBoard b;
    b.start = 0xFFFFF000u;  // wraps after 4096 frames (~68 s)
    const Result r = b.run(opts(600, 10));
    EXPECT_EQ(r.startCount, 0xFFFFF000u);
    EXPECT_EQ(r.endCount, 0xFFFFF000u + 36000u);  // wrapped value
    EXPECT_EQ(r.frames, 36000u);
    EXPECT_TRUE(r.passed());
}

TEST(FrameRate, MeasureOutOfRangeFails) {
    FakeBoard slow;
    slow.hz = 59.5;
    EXPECT_FALSE(slow.run(opts(600, 10)).passed());
    FakeBoard fast;
    fast.hz = 60.5;
    EXPECT_FALSE(fast.run(opts(600, 10)).passed());
}

TEST(FrameRate, StallIsReportedAndFails) {
    FakeBoard b;
    b.stallAfterFirst = true;
    std::vector<Sample> seen;
    const Result r = b.run(opts(30, 10), &seen);
    EXPECT_TRUE(r.stalled);
    EXPECT_FALSE(r.passed());
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0].interval, 600u);
    EXPECT_EQ(seen[1].interval, 0u);
    std::ostringstream out;
    printSample(out, seen[1]);
    EXPECT_NE(out.str().find("STALL"), std::string::npos);
}

// The last interval is shorter when --seconds is not a multiple of --sample.
TEST(FrameRate, ShortLastInterval) {
    FakeBoard b;
    std::vector<Sample> seen;
    const Result r = b.run(opts(25, 10), &seen);
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[2].ns, 25ull * 1000000000ull);
    EXPECT_EQ(seen[2].interval, 300u);
    EXPECT_EQ(r.frames, 1500u);
}

// The tool only reads the Frame Counter: no write, no other register.
TEST(FrameRate, ReadsOnlyTheFrameCounter) {
    FakeBoard b;
    static_cast<void>(b.run(opts(20, 10)));
    for (const auto& a : b.regs.log) {
        EXPECT_EQ(a.kind, arcade::hal::test::Access::Kind::kRead32);
        EXPECT_EQ(a.offset, kOffsetFrameCounter);
    }
}

// Analysis: App. A 1024x768 65 MHz / (1344 x 806) = 60.004 Hz; 640x480 800 x 525.
TEST(FrameRate, AnalysisImpliedPixelClock) {
    EXPECT_NEAR(impliedPixelClockHz(60.004, Mode::kXga1024), 65000000.0, 2000.0);
    EXPECT_TRUE(pixelClockInRange(impliedPixelClockHz(60.0, Mode::kXga1024), Mode::kXga1024));
    EXPECT_FALSE(pixelClockInRange(impliedPixelClockHz(59.5, Mode::kXga1024), Mode::kXga1024));
    EXPECT_DOUBLE_EQ(impliedPixelClockHz(60.0, Mode::kVga640), 25200000.0);
    EXPECT_TRUE(pixelClockInRange(25100000.0, Mode::kVga640));
    EXPECT_FALSE(pixelClockInRange(24900000.0, Mode::kVga640));
}

TEST(FrameRate, ReportShowsVerdictAndAnalysis) {
    FakeBoard b;
    const Result r = b.run(opts(600, 10));
    std::ostringstream out;
    printReport(out, r, Mode::kXga1024);
    const std::string s = out.str();
    EXPECT_NE(s.find("measured 60.0000 Hz"), std::string::npos) << s;
    EXPECT_NE(s.find("59.7000 to 60.3000"), std::string::npos) << s;
    EXPECT_NE(s.find("Analysis: implied pixel clock"), std::string::npos) << s;
    EXPECT_NE(s.find("PR-01 PASS"), std::string::npos) << s;
}

TEST(FrameRate, OptionsDefaultsAndValues) {
    std::string err;
    const Options d = parseOptions({}, err).value_or(opts(1, 1));
    EXPECT_EQ(d.seconds, 600u);
    EXPECT_EQ(d.sample, 10u);
    EXPECT_EQ(d.mode, Mode::kXga1024);
    const Options o =
        parseOptions({"--seconds", "60", "--sample", "5", "--mode", "640", "--help"}, err)
            .value_or(Options{});
    EXPECT_EQ(o.seconds, 60u);
    EXPECT_EQ(o.sample, 5u);
    EXPECT_EQ(o.mode, Mode::kVga640);
    EXPECT_TRUE(o.help);
    EXPECT_NE(usage().find("--seconds"), std::string::npos);
}

TEST(FrameRate, OptionsRejectBadInput) {
    std::string err;
    for (const auto& args : std::vector<std::vector<std::string>>{{"--seconds"},
                                                                  {"--seconds", "0"},
                                                                  {"--seconds", "86401"},
                                                                  {"--seconds", "-5"},
                                                                  {"--seconds", "1x"},
                                                                  {"--sample", "0"},
                                                                  {"--mode", "800"},
                                                                  {"--bogus"}}) {
        err.clear();
        EXPECT_FALSE(parseOptions(args, err)) << args[0];
        EXPECT_FALSE(err.empty());
    }
}
