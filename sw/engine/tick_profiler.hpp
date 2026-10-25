/// @file
/// Tick-time statistics for PR-04: tick count, 99.9th percentile and ticks over budget.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "arcade/tables.hpp"

namespace arcade {

/// Tick work budget in microseconds: 8.3 ms, half of a 16.7 ms frame (PR-04).
constexpr std::uint64_t kTickBudgetUs = 8'300;  // PR-04
/// Measurement run length in seconds: 10 minutes of each game (PR-03, PR-04 verification).
constexpr std::size_t kProfileRunSeconds = 600;  // PR-04
/// Percentile reported, as a fraction kPercentileNum / kPercentileDen = 99.9 % (PR-04).
constexpr std::uint64_t kPercentileNum = 999;    // PR-04
constexpr std::uint64_t kPercentileDen = 1'000;  // PR-04
/// Tick durations kept for the percentile: one 10-minute run at TICK_RATE (PR-04).
constexpr std::size_t kProfileCapacity = kProfileRunSeconds * TICK_RATE;

/// Summary of the recorded ticks (PR-04).
struct TickStats {
    std::uint64_t ticks = 0;       ///< Ticks recorded, including any beyond capacity.
    std::uint64_t p999Us = 0;      ///< 99.9th-percentile tick time (nearest rank), us.
    std::uint64_t overBudget = 0;  ///< Ticks longer than kTickBudgetUs.
    std::uint64_t maxUs = 0;       ///< Longest tick, us.
};

/// Records tick durations and computes PR-04's statistics. Storage is allocated once in the
/// constructor, never during a tick (R-4.1.6).
class TickProfiler {
  public:
    /// Reserves room for `capacity` durations (default: one 10-minute run, PR-04).
    explicit TickProfiler(std::size_t capacity = kProfileCapacity);

    /// Records one tick's work time (PR-04). Past capacity, the tick still counts towards
    /// ticks, overBudget and maxUs, but not towards the percentile.
    void record(std::uint64_t durationUs) noexcept;

    /// Computes the statistics; call outside the game loop (it sorts a copy).
    TickStats stats() const;

  private:
    std::vector<std::uint64_t> durations_;
    std::size_t capacity_;
    std::uint64_t ticks_ = 0;
    std::uint64_t overBudget_ = 0;
    std::uint64_t maxUs_ = 0;
};

}  // namespace arcade
