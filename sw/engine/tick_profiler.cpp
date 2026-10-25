// Tick-time statistics (PR-04).
#include "engine/tick_profiler.hpp"

#include <algorithm>

namespace arcade {

TickProfiler::TickProfiler(std::size_t capacity) : capacity_(capacity) {
    durations_.reserve(capacity_);
}

void TickProfiler::record(std::uint64_t durationUs) noexcept {
    ++ticks_;
    if (durationUs > kTickBudgetUs) {  // PR-04: 8.3 ms or less
        ++overBudget_;
    }
    maxUs_ = std::max(maxUs_, durationUs);
    if (durations_.size() < capacity_) {
        durations_.push_back(durationUs);  // capacity reserved: no allocation
    }
}

TickStats TickProfiler::stats() const {
    TickStats s;
    s.ticks = ticks_;
    s.overBudget = overBudget_;
    s.maxUs = maxUs_;
    if (durations_.empty()) {
        return s;
    }
    // PR-04: 99.9th percentile, nearest-rank method: rank = ceil(n * 999 / 1000).
    std::vector<std::uint64_t> sorted(durations_);
    const std::uint64_t n = sorted.size();
    const std::uint64_t rank = (n * kPercentileNum + kPercentileDen - 1) / kPercentileDen;
    const auto nth = sorted.begin() + static_cast<std::ptrdiff_t>(rank - 1);
    std::nth_element(sorted.begin(), nth, sorted.end());
    s.p999Us = *nth;
    return s;
}

}  // namespace arcade
