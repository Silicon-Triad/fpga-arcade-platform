/// @file
/// Monotonic clock used to time game ticks (PR-04). Injected so tests can use a fake clock.
#pragma once

#include <cstdint>

namespace arcade {

/// Source of monotonic time in microseconds (PR-04).
class TickClock {
  public:
    virtual ~TickClock() = default;

    /// Current monotonic time in microseconds; only differences are meaningful (PR-04).
    virtual std::uint64_t nowUs() noexcept = 0;
};

/// The real clock: clock_gettime(CLOCK_MONOTONIC), as PR-04's verification requires.
class MonotonicClock final : public TickClock {
  public:
    /// Reads CLOCK_MONOTONIC in microseconds (PR-04).
    std::uint64_t nowUs() noexcept override;
};

}  // namespace arcade
