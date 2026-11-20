/// @file
/// Opt-in key-to-swap latency measurement (`arcade --latency`, PR-02, issue #47).
///
/// LatencyEvdevSys wraps the real EvdevSys: it switches every opened event device to
/// CLOCK_MONOTONIC timestamps (EVIOCSCLOCKID) and copies the kernel timestamp of each key press
/// it reads into a LatencyRecorder, passing every event on unchanged. After each tick's
/// present() (the bank swap), the recorder turns the presses read during that tick into one
/// latency each: swap time - kernel event time. Nothing here exists unless --latency is given.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "input/evdev_sys.hpp"

namespace arcade {

/// PR-02: key press to the first displayed frame showing its effect, at most 67 ms.
constexpr std::uint64_t kPr02LimitUs = 67000;
/// App. A FRAME_PERIOD_MS = 16.67 ms (PR-02, PR-03): the scan-out after a swap is at most one
/// frame (PR-05).
constexpr std::uint64_t kFramePeriodUs = 16670;
/// DESIGN CHOICE, NOT AN SRS VALUE: a press older than 1 s at its swap, or later than the swap,
/// means the event clock is not CLOCK_MONOTONIC; the run then gives no verdict.
constexpr std::uint64_t kClockSanityUs = 1000000;
/// DESIGN CHOICE: presses held between two swaps; more are counted as dropped.
constexpr std::size_t kMaxPendingPresses = 64;
/// DESIGN CHOICE: latencies kept for the p99; max and count cover every press.
constexpr std::size_t kMaxLatencySamples = 8192;

/// Summary of a latency run.
struct LatencySummary {
    std::uint64_t count = 0;  ///< presses measured
    std::uint64_t maxUs = 0;  ///< max(b)
    std::uint64_t p99Us = 0;  ///< 99th percentile of b over the kept samples
    std::uint64_t dropped = 0;
    bool clockOk = true;  ///< every EVIOCSCLOCKID succeeded and every timestamp was sane
    /// a + max(b) + one frame (PR-02 analysis).
    std::uint64_t boundUs(std::uint64_t usbUs) const noexcept {
        return usbUs + maxUs + kFramePeriodUs;
    }
};

/// Collects press timestamps and turns them into latencies at each swap (PR-02).
class LatencyRecorder {
  public:
    /// `log` receives one line per press; it may be nullptr.
    explicit LatencyRecorder(std::FILE* log) : log_(log) { samples_.reserve(kMaxLatencySamples); }

    /// A key press with kernel timestamp `eventUs` (CLOCK_MONOTONIC) was read.
    void onPress(std::uint64_t eventUs) noexcept;
    /// EVIOCSCLOCKID failed on an opened device: no verdict.
    void onClockSetFailed() noexcept { clockOk_ = false; }
    /// present() of the tick that read the pending presses returned at `swapUs`.
    void onSwap(std::uint64_t swapUs) noexcept;

    /// Count, max, p99, dropped and the clock check.
    LatencySummary summary() const;

  private:
    std::FILE* log_;
    std::array<std::uint64_t, kMaxPendingPresses> pending_{};
    std::size_t pendingCount_ = 0;
    std::vector<std::uint64_t> samples_;
    std::uint64_t count_ = 0;
    std::uint64_t maxUs_ = 0;
    std::uint64_t dropped_ = 0;
    bool clockOk_ = true;
};

/// Prints the summary and the PR-02 verdict for USB polling bound `usbUs` (part a, from the
/// keyboard's bInterval in `lsusb -v`).
void printLatencySummary(const LatencySummary& s, std::uint64_t usbUs, std::FILE* log);

/// EvdevSys decorator that records key-press timestamps (PR-02). Forwards every call.
class LatencyEvdevSys final : public EvdevSys {
  public:
    /// Sets CLOCK_MONOTONIC timestamps on `fd`; false on failure. Injected for tests.
    using SetClockFn = std::function<bool(int fd)>;

    LatencyEvdevSys(EvdevSys& inner, LatencyRecorder& recorder, SetClockFn setClock);

    std::vector<std::string> listDir(const std::string& dir) override;
    /// Opens, then switches the device to CLOCK_MONOTONIC timestamps (EVIOCSCLOCKID).
    int open(const std::string& path, int flags) override;
    void close(int fd) override;
    bool keyBits(int fd, KeyBits& bits) override;
    bool grab(int fd) override;
    /// Reads, then records the timestamp of each EV_KEY press (value 1) in the data.
    ssize_t read(int fd, void* buf, std::size_t len, int& err) override;
    int watchDir(const std::string& dir) override;
    void readDirChanges(int watchFd, DirChangeSink& sink) override;

  private:
    EvdevSys& inner_;
    LatencyRecorder& recorder_;
    SetClockFn setClock_;
};

/// The real SetClockFn: `ioctl(fd, EVIOCSCLOCKID, &CLOCK_MONOTONIC)`.
bool setMonotonicClock(int fd) noexcept;

}  // namespace arcade
