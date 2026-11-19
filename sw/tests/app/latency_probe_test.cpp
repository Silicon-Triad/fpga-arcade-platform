#include "app/latency_probe.hpp"

#include <gtest/gtest.h>
#include <linux/input.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "app/options.hpp"

namespace arcade {
namespace {

/// Fake evdev: open() returns fd 3, read() returns the queued events once.
class FakeEvdev final : public EvdevSys {
  public:
    std::vector<std::string> listDir(const std::string& /*dir*/) override { return {"event0"}; }
    int open(const std::string& path, int /*flags*/) override {
        opened.push_back(path);
        return 3;
    }
    void close(int fd) override { closed.push_back(fd); }
    bool keyBits(int /*fd*/, KeyBits& bits) override {
        bits.fill(0xFF);
        return true;
    }
    bool grab(int /*fd*/) override { return true; }
    ssize_t read(int /*fd*/, void* buf, std::size_t len, int& err) override {
        if (queue.empty()) {
            err = EAGAIN;
            return -1;
        }
        const std::size_t bytes = queue.size() * sizeof(input_event);
        EXPECT_LE(bytes, len);
        std::memcpy(buf, queue.data(), bytes);
        queue.clear();
        return static_cast<ssize_t>(bytes);
    }
    int watchDir(const std::string& /*dir*/) override { return 7; }
    void readDirChanges(int /*watchFd*/, DirChangeSink& /*sink*/) override { ++dirReads; }

    std::vector<input_event> queue;
    std::vector<std::string> opened;
    std::vector<int> closed;
    int dirReads = 0;
};

input_event keyEvent(std::uint64_t us, std::uint16_t type, std::int32_t value) {
    input_event e{};
    e.input_event_sec = static_cast<decltype(e.input_event_sec)>(us / 1000000);
    e.input_event_usec = static_cast<decltype(e.input_event_usec)>(us % 1000000);
    e.type = type;
    e.code = KEY_UP;
    e.value = value;
    return e;
}

std::string readAll(std::FILE* f) {
    (void)std::fseek(f, 0, SEEK_SET);
    std::string s;
    std::array<char, 256> buf{};
    while (std::fgets(buf.data(), static_cast<int>(buf.size()), f) != nullptr) {
        s += buf.data();
    }
    return s;
}

}  // namespace

// PR-02 (#47): only presses (EV_KEY value 1) are recorded; events pass through unchanged.
TEST(LatencyProbe, PR_02_RecordsOnlyPressesAndPassesEventsOn) {
    FakeEvdev inner;
    LatencyRecorder rec(nullptr);
    std::vector<int> clockFds;
    LatencyEvdevSys sys(inner, rec, [&clockFds](int fd) {
        clockFds.push_back(fd);
        return true;
    });
    EXPECT_EQ(sys.open("/dev/input/event0", 0), 3);
    EXPECT_EQ(clockFds, std::vector<int>{3});  // EVIOCSCLOCKID on the opened fd

    inner.queue = {keyEvent(1000000, EV_KEY, 1), keyEvent(1001000, EV_KEY, 0),
                   keyEvent(1002000, EV_KEY, 2), keyEvent(1003000, EV_SYN, 1),
                   keyEvent(1004000, EV_KEY, 1)};
    const std::vector<input_event> sent = inner.queue;
    std::array<input_event, 8> buf{};
    int err = 0;
    const ssize_t n = sys.read(3, buf.data(), sizeof(buf), err);
    ASSERT_EQ(n, static_cast<ssize_t>(5 * sizeof(input_event)));
    EXPECT_EQ(std::memcmp(buf.data(), sent.data(), static_cast<std::size_t>(n)), 0);

    rec.onSwap(1020000);  // swap 20 ms after the first press, 16 ms after the second
    const LatencySummary s = rec.summary();
    EXPECT_EQ(s.count, 2u);
    EXPECT_EQ(s.maxUs, 20000u);
    EXPECT_TRUE(s.clockOk);
}

TEST(LatencyProbe, ForwardsEveryOtherCall) {
    FakeEvdev inner;
    LatencyRecorder rec(nullptr);
    LatencyEvdevSys sys(inner, rec, [](int) { return true; });
    EXPECT_EQ(sys.listDir("/dev/input"), std::vector<std::string>{"event0"});
    KeyBits bits{};
    EXPECT_TRUE(sys.keyBits(3, bits));
    EXPECT_EQ(bits[0], 0xFF);
    EXPECT_TRUE(sys.grab(3));
    EXPECT_EQ(sys.watchDir("/dev/input"), 7);
    struct NullSink final : DirChangeSink {
        void entryChanged(const char* /*name*/) override {}
        void eventsLost() override {}
    } sink;
    sys.readDirChanges(7, sink);
    EXPECT_EQ(inner.dirReads, 1);
    sys.close(3);
    EXPECT_EQ(inner.closed, std::vector<int>{3});
    int err = 0;
    std::array<input_event, 1> buf{};
    EXPECT_EQ(sys.read(3, buf.data(), sizeof(buf), err), -1);
    EXPECT_EQ(err, EAGAIN);
    EXPECT_EQ(rec.summary().count, 0u);
}

TEST(LatencyProbe, ClockSetFailureMeansNoVerdict) {
    FakeEvdev inner;
    LatencyRecorder rec(nullptr);
    LatencyEvdevSys sys(inner, rec, [](int) { return false; });
    static_cast<void>(sys.open("/dev/input/event0", 0));
    rec.onPress(1000);
    rec.onSwap(2000);
    EXPECT_FALSE(rec.summary().clockOk);
    std::FILE* f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    printLatencySummary(rec.summary(), 8000, f);
    EXPECT_NE(readAll(f).find("clock check FAILED: no PR-02 verdict"), std::string::npos);
    static_cast<void>(std::fclose(f));
}

// A timestamp after the swap, or more than 1 s before it, is not CLOCK_MONOTONIC.
TEST(LatencyProbe, ImplausibleTimestampsFailTheClockCheck) {
    LatencyRecorder later(nullptr);
    later.onPress(5000);
    later.onSwap(4000);
    EXPECT_FALSE(later.summary().clockOk);
    EXPECT_EQ(later.summary().count, 0u);

    LatencyRecorder stale(nullptr);
    stale.onPress(1000);
    stale.onSwap(1000 + kClockSanityUs + 1);
    EXPECT_FALSE(stale.summary().clockOk);

    LatencyRecorder edge(nullptr);
    edge.onPress(1000);
    edge.onSwap(1000 + kClockSanityUs);
    EXPECT_TRUE(edge.summary().clockOk);
}

TEST(LatencyProbe, PendingPressesClearAtEachSwap) {
    LatencyRecorder rec(nullptr);
    rec.onPress(1000);
    rec.onSwap(11000);
    rec.onSwap(50000);  // nothing pending: no new sample
    EXPECT_EQ(rec.summary().count, 1u);
    EXPECT_EQ(rec.summary().maxUs, 10000u);
}

TEST(LatencyProbe, OverflowIsCountedAsDropped) {
    LatencyRecorder rec(nullptr);
    for (std::size_t i = 0; i < kMaxPendingPresses + 3; ++i) {
        rec.onPress(1000);
    }
    rec.onSwap(2000);
    EXPECT_EQ(rec.summary().count, kMaxPendingPresses);
    EXPECT_EQ(rec.summary().dropped, 3u);
}

// p99 by nearest rank: 100 samples 1..100 ms -> 99 ms.
TEST(LatencyProbe, P99NearestRank) {
    LatencyRecorder rec(nullptr);
    for (std::uint64_t ms = 1; ms <= 100; ++ms) {
        rec.onPress(1000000);
        rec.onSwap(1000000 + ms * 1000);
    }
    const LatencySummary s = rec.summary();
    EXPECT_EQ(s.count, 100u);
    EXPECT_EQ(s.p99Us, 99000u);
    EXPECT_EQ(s.maxUs, 100000u);
}

// PR-02: a + max(b) + one frame (App. A FRAME_PERIOD_MS) <= 67 ms.
TEST(LatencyProbe, PR_02_BoundAndVerdict) {
    EXPECT_EQ(kPr02LimitUs, 67000u);
    EXPECT_EQ(kFramePeriodUs, 16670u);
    LatencySummary s;
    s.count = 1;
    s.maxUs = 42330;  // 8 + 42.33 + 16.67 = 67.00 ms: PASS at the limit
    EXPECT_EQ(s.boundUs(8000), 67000u);

    std::FILE* f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    printLatencySummary(s, 8000, f);
    s.maxUs = 42331;
    printLatencySummary(s, 8000, f);
    const std::string out = readAll(f);
    static_cast<void>(std::fclose(f));
    EXPECT_NE(out.find("= 67.000 ms (PR-02 limit 67 ms): PASS"), std::string::npos) << out;
    EXPECT_NE(out.find("= 67.001 ms (PR-02 limit 67 ms): FAIL"), std::string::npos) << out;
}

TEST(LatencyProbe, NoPressesMeansNoVerdict) {
    std::FILE* f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    printLatencySummary(LatencySummary{}, 8000, f);
    EXPECT_NE(readAll(f).find("no presses measured"), std::string::npos);
    static_cast<void>(std::fclose(f));
}

TEST(LatencyProbe, PerPressLineIsPrinted) {
    std::FILE* f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    LatencyRecorder rec(f);
    rec.onPress(1000);
    rec.onSwap(13500);
    EXPECT_NE(readAll(f).find("latency: press 1 b=12.500 ms"), std::string::npos);
    static_cast<void>(std::fclose(f));
}

namespace {
template <std::size_t N>
std::optional<Options> parseArgs(const std::array<const char*, N>& args) {
    return parseOptions(static_cast<int>(N), args.data(), BackendKind::Fpga);
}
}  // namespace

TEST(LatencyOptions, LatencyNeedsUsbMsAndFpga) {
    const Options none{};
    const auto ok = parseArgs(std::array<const char*, 3>{"arcade", "--latency", "--usb-ms=8"});
    EXPECT_TRUE(ok.value_or(none).latency);
    EXPECT_EQ(ok.value_or(none).usbMs, 8u);
    EXPECT_FALSE(parseArgs(std::array<const char*, 2>{"arcade", "--latency"}));
    EXPECT_FALSE(parseArgs(
        std::array<const char*, 4>{"arcade", "--backend=sdl", "--latency", "--usb-ms=8"}));
    EXPECT_FALSE(parseArgs(std::array<const char*, 2>{"arcade", "--usb-ms=0"}));
    EXPECT_FALSE(parseArgs(std::array<const char*, 2>{"arcade", "--usb-ms=256"}));
    EXPECT_FALSE(parseArgs(std::array<const char*, 2>{"arcade", "--usb-ms=8x"}));
    EXPECT_FALSE(parseArgs(std::array<const char*, 2>{"arcade", "--usb-ms="}));
    EXPECT_FALSE(parseArgs(std::array<const char*, 1>{"arcade"}).value_or(none).latency);
}

}  // namespace arcade
