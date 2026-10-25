// Real tick clock (PR-04).
#include "engine/tick_clock.hpp"

#include <ctime>

namespace arcade {

namespace {
constexpr std::uint64_t kUsPerSecond = 1'000'000;
constexpr std::uint64_t kNsPerUs = 1'000;
}  // namespace

std::uint64_t MonotonicClock::nowUs() noexcept {
    // PR-04: time each tick with clock_gettime(CLOCK_MONOTONIC).
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * kUsPerSecond +
           static_cast<std::uint64_t>(ts.tv_nsec) / kNsPerUs;
}

}  // namespace arcade
