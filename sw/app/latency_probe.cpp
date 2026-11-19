#include "app/latency_probe.hpp"

#include <linux/input.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <ctime>
#include <utility>

namespace arcade {
namespace {

constexpr std::uint64_t kUsPerSecond = 1000000;
constexpr std::uint64_t kUsPerMs = 1000;
constexpr int kPressValue = 1;  // SR-INP-05: input_event.value 1 is a press

double ms(std::uint64_t us) { return static_cast<double>(us) / static_cast<double>(kUsPerMs); }

}  // namespace

void LatencyRecorder::onPress(std::uint64_t eventUs) noexcept {
    if (pendingCount_ == pending_.size()) {
        ++dropped_;
        return;
    }
    pending_[pendingCount_++] = eventUs;
}

void LatencyRecorder::onSwap(std::uint64_t swapUs) noexcept {
    for (std::size_t i = 0; i < pendingCount_; ++i) {
        const std::uint64_t eventUs = pending_[i];
        if (eventUs > swapUs || swapUs - eventUs > kClockSanityUs) {
            clockOk_ = false;  // not CLOCK_MONOTONIC, or a stale event
            if (log_ != nullptr) {
                (void)std::fprintf(log_,
                                   "latency: clock check FAILED (event %" PRIu64
                                   " us, swap %" PRIu64 " us)\n",
                                   eventUs, swapUs);
            }
            continue;
        }
        const std::uint64_t b = swapUs - eventUs;
        ++count_;
        maxUs_ = std::max(maxUs_, b);
        if (samples_.size() < kMaxLatencySamples) {
            samples_.push_back(b);  // capacity reserved: no allocation here
        }
        if (log_ != nullptr) {
            (void)std::fprintf(log_, "latency: press %" PRIu64 " b=%.3f ms\n", count_, ms(b));
        }
    }
    pendingCount_ = 0;
}

LatencySummary LatencyRecorder::summary() const {
    LatencySummary s;
    s.count = count_;
    s.maxUs = maxUs_;
    s.dropped = dropped_;
    s.clockOk = clockOk_;
    if (!samples_.empty()) {
        std::vector<std::uint64_t> sorted = samples_;
        std::sort(sorted.begin(), sorted.end());
        // Nearest rank: ceil(0.99 * n), 1-based.
        const std::size_t rank = (sorted.size() * 99 + 99) / 100;
        s.p99Us = sorted[rank - 1];
    }
    return s;
}

void printLatencySummary(const LatencySummary& s, std::uint64_t usbUs, std::FILE* log) {
    (void)std::fprintf(log,
                       "latency: n=%" PRIu64 " max(b)=%.3f ms p99(b)=%.3f ms dropped=%" PRIu64 "\n",
                       s.count, ms(s.maxUs), ms(s.p99Us), s.dropped);
    if (!s.clockOk) {
        (void)std::fprintf(log, "latency: clock check FAILED: no PR-02 verdict\n");
        return;
    }
    if (s.dropped > 0) {
        // max(b) would leave the dropped presses out (review of #127).
        (void)std::fprintf(log, "latency: presses dropped: no PR-02 verdict\n");
        return;
    }
    if (s.count == 0) {
        (void)std::fprintf(log, "latency: no presses measured: no PR-02 verdict\n");
        return;
    }
    const std::uint64_t bound = s.boundUs(usbUs);
    (void)std::fprintf(log,
                       "latency: bound = a %.3f ms + max(b) %.3f ms + one frame %.3f ms = %.3f ms"
                       " (PR-02 limit %.0f ms): %s\n",
                       ms(usbUs), ms(s.maxUs), ms(kFramePeriodUs), ms(bound), ms(kPr02LimitUs),
                       bound <= kPr02LimitUs ? "PASS" : "FAIL");
}

LatencyEvdevSys::LatencyEvdevSys(EvdevSys& inner, LatencyRecorder& recorder, SetClockFn setClock)
    : inner_(inner), recorder_(recorder), setClock_(std::move(setClock)) {}

std::vector<std::string> LatencyEvdevSys::listDir(const std::string& dir) {
    return inner_.listDir(dir);
}

int LatencyEvdevSys::open(const std::string& path, int flags) {
    const int fd = inner_.open(path, flags);
    if (fd >= 0 && !setClock_(fd)) {
        recorder_.onClockSetFailed();
    }
    return fd;
}

void LatencyEvdevSys::close(int fd) { inner_.close(fd); }

bool LatencyEvdevSys::keyBits(int fd, KeyBits& bits) { return inner_.keyBits(fd, bits); }

bool LatencyEvdevSys::grab(int fd) { return inner_.grab(fd); }

ssize_t LatencyEvdevSys::read(int fd, void* buf, std::size_t len, int& err) {
    const ssize_t bytes = inner_.read(fd, buf, len, err);
    if (bytes > 0) {
        const std::size_t n = static_cast<std::size_t>(bytes) / sizeof(input_event);
        const auto* bytesIn = static_cast<const unsigned char*>(buf);
        for (std::size_t i = 0; i < n; ++i) {
            input_event ev{};
            std::memcpy(&ev, bytesIn + i * sizeof(input_event), sizeof(input_event));
            if (ev.type == EV_KEY && ev.value == kPressValue) {
                recorder_.onPress(static_cast<std::uint64_t>(ev.input_event_sec) * kUsPerSecond +
                                  static_cast<std::uint64_t>(ev.input_event_usec));
            }
        }
    }
    return bytes;
}

int LatencyEvdevSys::watchDir(const std::string& dir) { return inner_.watchDir(dir); }

void LatencyEvdevSys::readDirChanges(int watchFd, DirChangeSink& sink) {
    inner_.readDirChanges(watchFd, sink);
}

bool setMonotonicClock(int fd) noexcept {
    // EVIOCSCLOCKID (include/uapi/linux/input.h, drivers/input/evdev.c evdev_set_clk_type()):
    // evdev timestamps default to CLOCK_REALTIME; this switches the device to CLOCK_MONOTONIC.
    // The swap time it is compared with comes from arcade::MonotonicClock::nowUs()
    // (sw/engine/tick_clock.cpp), which calls clock_gettime(CLOCK_MONOTONIC): same clock.
    int clk = CLOCK_MONOTONIC;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): ioctl is variadic.
    return ::ioctl(fd, EVIOCSCLOCKID, &clk) == 0;
}

}  // namespace arcade
