#include "hal/frame_rate.hpp"

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "hal/register_map.hpp"

namespace arcade::hal::fps {
namespace {

constexpr std::uint32_t kMaxSeconds = 86400;  // tool limit: one day
constexpr std::uint32_t kMaxSample = 3600;    // tool limit: one hour
constexpr double kNsPerSecond = 1e9;

/// Parses a decimal number in [lo, hi]; false on anything else.
bool parseUint(const std::string& text, std::uint32_t lo, std::uint32_t hi, std::uint32_t& out) {
    if (text.empty() || text.size() > 10 ||
        !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return false;
    }
    const unsigned long long v = std::strtoull(text.c_str(), nullptr, 10);
    if (v < lo || v > hi) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

std::uint64_t toNs(std::chrono::nanoseconds d) noexcept {
    return d.count() > 0 ? static_cast<std::uint64_t>(d.count()) : 0;
}

}  // namespace

std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error) {
    Options opt;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--help" || a == "-h") {
            opt.help = true;
            continue;
        }
        const bool takesValue = a == "--seconds" || a == "--sample" || a == "--mode";
        if (!takesValue) {
            error = "unknown argument '" + a + "'";
            return std::nullopt;
        }
        if (i + 1 >= args.size()) {
            error = a + " needs a value";
            return std::nullopt;
        }
        const std::string& v = args[++i];
        if (a == "--seconds") {
            if (!parseUint(v, 1, kMaxSeconds, opt.seconds)) {
                error = "--seconds must be 1.." + std::to_string(kMaxSeconds);
                return std::nullopt;
            }
        } else if (a == "--sample") {
            if (!parseUint(v, 1, kMaxSample, opt.sample)) {
                error = "--sample must be 1.." + std::to_string(kMaxSample);
                return std::nullopt;
            }
        } else if (v == "1024") {
            opt.mode = Mode::kXga1024;
        } else if (v == "640") {
            opt.mode = Mode::kVga640;
        } else {
            error = "--mode must be 1024 or 640";
            return std::nullopt;
        }
    }
    return opt;
}

std::string usage() {
    return "usage: frame_rate [--seconds N] [--sample N] [--mode 1024|640] [--help]\n"
           "  Counts VGA frames with the Frame Counter register against CLOCK_MONOTONIC\n"
           "  and checks PR-01 (59.7 Hz to 60.3 Hz). Board only, as root.\n"
           "  --seconds N  measurement length, default 600\n"
           "  --sample N   seconds between progress lines, default 10\n"
           "  --mode M     video mode of the loaded .rbf, for the Analysis line, default 1024\n";
}

std::uint32_t countDelta(std::uint32_t start, std::uint32_t end) noexcept {
    return end - start;  // unsigned arithmetic is modulo 2^32: one wrap is handled
}

double measuredHz(std::uint64_t frames, std::uint64_t ns) noexcept {
    if (ns == 0) {
        return 0.0;
    }
    return static_cast<double>(frames) * kNsPerSecond / static_cast<double>(ns);
}

bool meetsPr01(double hz) noexcept { return hz >= kPr01MinHz && hz <= kPr01MaxHz; }

double impliedPixelClockHz(double hz, Mode mode) noexcept {
    const bool xga = mode == Mode::kXga1024;
    const double h = xga ? kHTotal : kBringupHTotal;
    const double v = xga ? kVTotal : kBringupVTotal;
    return hz * h * v;
}

bool pixelClockInRange(double pixelHz, Mode mode) noexcept {
    if (mode == Mode::kXga1024) {
        return pixelHz >= kPixelClkHz * (1.0 - kPixelClkTolerance) &&
               pixelHz <= kPixelClkHz * (1.0 + kPixelClkTolerance);
    }
    return pixelHz >= kBringupPixelClkMinHz && pixelHz <= kBringupPixelClkMaxHz;
}

Result measure(RegisterSpace& regs, MonotonicClock& clock, const SleepFn& sleep, const Options& opt,
               const std::function<void(const Sample&)>& onSample) {
    Result r;
    // Counter first, then time, at every read, so each pair is taken the same way.
    r.startCount = regs.read32(kOffsetFrameCounter);  // §3.1.4, SR-HAL-06
    const auto t0 = clock.now();
    const std::uint64_t totalNs = std::uint64_t{opt.seconds} * 1000000000ull;
    const std::uint64_t stepNs = std::uint64_t{opt.sample} * 1000000000ull;

    std::uint32_t prevCount = r.startCount;
    std::uint64_t prevNs = 0;
    std::uint64_t now = 0;
    while (now < totalNs) {
        const std::uint64_t target = std::min(prevNs + stepNs, totalNs);
        sleep(std::chrono::nanoseconds(static_cast<std::int64_t>(target - now)));
        const std::uint32_t count = regs.read32(kOffsetFrameCounter);
        now = toNs(clock.now() - t0);
        if (now < target) {
            continue;  // woke early: sleep the rest before sampling
        }
        Sample s;
        s.ns = now;
        s.counter = count;
        s.interval = countDelta(prevCount, count);  // per interval, so any number of wraps
        s.frames = r.frames + s.interval;
        s.intervalHz = measuredHz(s.interval, now - prevNs);
        r.frames = s.frames;
        r.stalled = r.stalled || s.interval == 0;
        r.samples.push_back(s);
        onSample(s);
        prevCount = count;
        prevNs = now;
    }
    r.endCount = prevCount;
    r.ns = now;
    r.hz = measuredHz(r.frames, r.ns);
    return r;
}

void printSample(std::ostream& out, const Sample& s) {
    out << std::fixed << std::setprecision(3) << "t=" << static_cast<double>(s.ns) / kNsPerSecond
        << " s count=" << s.counter << " frames=" << s.frames << " interval=" << s.interval << " ("
        << s.intervalHz << " Hz)" << (s.interval == 0 ? " STALL" : "") << "\n";
}

void printReport(std::ostream& out, const Result& r, Mode mode) {
    const bool xga = mode == Mode::kXga1024;
    const double pix = impliedPixelClockHz(r.hz, mode);
    out << std::fixed << std::setprecision(3) << "start count " << r.startCount << ", end count "
        << r.endCount << ", frames " << r.frames << "\n"
        << "elapsed " << static_cast<double>(r.ns) / kNsPerSecond << " s (CLOCK_MONOTONIC)\n"
        << std::setprecision(4) << "measured " << r.hz << " Hz; PR-01 limits " << kPr01MinHz
        << " to " << kPr01MaxHz << " Hz\n";
    if (r.stalled) {
        out << "Frame Counter stalled in at least one interval\n";
    }
    out << std::setprecision(0) << "Analysis: implied pixel clock " << pix << " Hz = Hz x "
        << (xga ? kHTotal : kBringupHTotal) << " x " << (xga ? kVTotal : kBringupVTotal)
        << (xga ? " (SR-VGA-02/03); App. A PIXEL_CLK_HZ 65000000 +/- 0.5 %"
                : " (SR-VGA-10); App. A BRINGUP_PIXEL_CLK_HZ 25000000 to 25200000")
        << (pixelClockInRange(pix, mode) ? ": in range\n" : ": out of range\n") << "PR-01 "
        << (r.passed() ? "PASS" : "FAIL") << "\n";
}

}  // namespace arcade::hal::fps
