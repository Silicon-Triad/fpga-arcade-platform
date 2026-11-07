#pragma once

#include <chrono>
#include <cstdint>

#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"

namespace arcade::hal {

/// Frame rate used for the §3.1.4 worst-case data rate: 60 Hz (§3.1.4 "Data rate", SR-FW-02).
inline constexpr std::uint32_t kBenchFrameRateHz = 60;  // §3.1.4
/// §3.1.4 "Data rate": worst-case load of rewriting the whole tilemap bank every frame,
/// 12,288 x 60 = 737,280 bytes/s ("approximately 737 kB/s"). Issue #38 verdict target.
inline constexpr std::uint64_t kWorstCaseTilemapBytesPerSecond =
    static_cast<std::uint64_t>(TILEMAP_BANK_SIZE) * kBenchFrameRateHz;  // §3.1.4, PR-04

/// Result of one LW-bridge tilemap write benchmark run (issue #38, §3.1.4 open item).
struct BridgeBenchResult {
    std::uint64_t bytesWritten = 0;       ///< Total tilemap bytes written over the bridge.
    std::chrono::nanoseconds elapsed{0};  ///< Wall time for all passes.
    std::uint64_t bytesPerSecond = 0;     ///< bytesWritten / elapsed (0 if elapsed is 0).
    std::uint32_t backBankOffset = 0;     ///< Offset of the bank that was written.
    bool meetsWorstCase = false;          ///< bytesPerSecond >= kWorstCaseTilemapBytesPerSecond.
};

/// §3.1.4 open item, PR-04 (issue #38): time `passes` full-bank writes (TILEMAP_BANK_SIZE
/// bytes each, one write8 per cell, the same access pattern as present() in SR-HAL-08) into
/// the back bank, i.e. the bank that Front Bank says is not on screen, so nothing visible
/// changes. Reads Front Bank once before timing; writes no other register and requests no
/// swap. passes == 0 writes nothing and returns a zero result.
BridgeBenchResult benchmarkTilemapWrites(RegisterSpace& regs, MonotonicClock& clock,
                                         std::uint32_t passes) noexcept;

}  // namespace arcade::hal
