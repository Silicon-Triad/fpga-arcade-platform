#pragma once

// VGA frame-rate measurement (#46): the parts that need no board, so they can be unit-tested
// on the PC. The board program is frame_rate_main.cpp.
// SRS: PR-01, SR-VGA-10 (method changed: Frame Counter against CLOCK_MONOTONIC instead of a
// scope or frequency counter on VGA_VS; see sw/hal/README.md).

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"

namespace arcade::hal::fps {

/// PR-01: lowest accepted frame rate, 60 Hz - 0.5 %.
inline constexpr double kPr01MinHz = 59.7;
/// PR-01: highest accepted frame rate, 60 Hz + 0.5 %.
inline constexpr double kPr01MaxHz = 60.3;

/// App. A H_TOTAL (SR-VGA-02): pixel clocks per line, 1024x768 mode.
inline constexpr std::uint32_t kHTotal = 1344;
/// App. A V_TOTAL (SR-VGA-03): lines per frame, 1024x768 mode.
inline constexpr std::uint32_t kVTotal = 806;
/// App. A PIXEL_CLK_HZ (SR-VGA-01): 65,000,000 Hz.
inline constexpr double kPixelClkHz = 65000000.0;
/// App. A PIXEL_CLK_HZ tolerance (SR-VGA-01): +/- 0.5 %.
inline constexpr double kPixelClkTolerance = 0.005;
/// App. A BRINGUP_H_TOTAL (SR-VGA-10): pixel clocks per line, 640x480 mode.
inline constexpr std::uint32_t kBringupHTotal = 800;
/// App. A BRINGUP_V_TOTAL (SR-VGA-10): lines per frame, 640x480 mode.
inline constexpr std::uint32_t kBringupVTotal = 525;
/// App. A BRINGUP_PIXEL_CLK_HZ (SR-VGA-10): lower limit, 25,000,000 Hz.
inline constexpr double kBringupPixelClkMinHz = 25000000.0;
/// App. A BRINGUP_PIXEL_CLK_HZ (SR-VGA-10): upper limit, 25,200,000 Hz.
inline constexpr double kBringupPixelClkMaxHz = 25200000.0;

/// Video mode of the loaded design (App. A VIDEO_MODE, SR-VGA-11). Only the Analysis line
/// depends on it; the PR-01 verdict is the same for both.
enum class Mode : std::uint8_t {
    kXga1024,  ///< 1024x768 operating mode
    kVga640,   ///< 640x480 bring-up mode (SR-VGA-10)
};

/// Command-line options of frame_rate.
struct Options {
    std::uint32_t seconds = 600;  ///< measurement length; 600 s: one count is ~0.003 %
    std::uint32_t sample = 10;    ///< seconds between progress samples
    Mode mode = Mode::kXga1024;
    bool help = false;
};

/// Parses `--seconds N` (1..86400), `--sample N` (1..3600), `--mode 1024|640` and `--help`.
/// Returns std::nullopt and sets `error` on a bad argument.
std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error);

/// Usage text for --help and for a bad argument.
std::string usage();

/// Frames between two reads of the 32-bit Frame Counter (§3.1.4, SR-HAL-06), correct across
/// one wrap from 0xFFFFFFFF to 0 (modulo 2^32).
std::uint32_t countDelta(std::uint32_t start, std::uint32_t end) noexcept;

/// Frames per second for `frames` counted in `ns` nanoseconds; 0 when `ns` is 0.
double measuredHz(std::uint64_t frames, std::uint64_t ns) noexcept;

/// PR-01: true when `hz` is within 59.7 Hz to 60.3 Hz, both limits included.
bool meetsPr01(double hz) noexcept;

/// Analysis (not a measurement): the pixel clock implied by `hz` and the H_TOTAL x V_TOTAL of
/// `mode` (SR-VGA-02/03 or SR-VGA-10).
double impliedPixelClockHz(double hz, Mode mode) noexcept;

/// Analysis: true when `pixelHz` is inside the App. A pixel-clock range of `mode`.
bool pixelClockInRange(double pixelHz, Mode mode) noexcept;

/// One progress sample.
struct Sample {
    std::uint64_t ns = 0;        ///< nanoseconds since the start read
    std::uint32_t counter = 0;   ///< raw Frame Counter value
    std::uint64_t frames = 0;    ///< frames since the start read (wraps accounted)
    std::uint32_t interval = 0;  ///< frames since the previous sample
    double intervalHz = 0.0;     ///< rate over the last interval
};

/// Result of one measurement.
struct Result {
    std::uint32_t startCount = 0;
    std::uint32_t endCount = 0;
    std::uint64_t frames = 0;  ///< total frames counted (wraps accounted)
    std::uint64_t ns = 0;      ///< total elapsed nanoseconds
    double hz = 0.0;
    bool stalled = false;  ///< some sample interval counted no frame
    std::vector<Sample> samples;
    /// PR-01 verdict: in range and no stall.
    bool passed() const noexcept { return !stalled && meetsPr01(hz); }
};

/// Waits the given time (nanosleep on the board; a fake in tests).
using SleepFn = std::function<void(std::chrono::nanoseconds)>;

/// Reads the Frame Counter and the clock at the start, then every `opt.sample` seconds until
/// `opt.seconds` have passed (the last interval may be shorter), calling `onSample` after each
/// read so progress shows on the console. Reads only the Frame Counter register.
Result measure(RegisterSpace& regs, MonotonicClock& clock, const SleepFn& sleep, const Options& opt,
               const std::function<void(const Sample&)>& onSample);

/// One progress line, e.g. `t=10.000 s count=1234 frames=600 interval=600 (60.000 Hz)`, with
/// ` STALL` added when the interval counted no frame.
void printSample(std::ostream& out, const Sample& s);

/// The final report: start/end counts, elapsed, measured Hz, PR-01 limits, the Analysis line
/// for `mode`, and `PASS` or `FAIL`.
void printReport(std::ostream& out, const Result& r, Mode mode);

}  // namespace arcade::hal::fps
