#pragma once

// LW-bridge write throughput benchmark (#38): the parts that need no board, so they can be
// unit-tested on the PC. The board program is bridge_benchmark_main.cpp.
// SRS: §3.1.4 data-rate open item, PR-04.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hal/register_map.hpp"

namespace arcade::hal::bench {

/// Game ticks per second (App. A TICK_RATE, SR-FW-02).
inline constexpr std::uint64_t kTickRateHz = 60;

/// Worst-case bridge write load: the whole 12,288-byte tilemap every tick, 12,288 x 60 =
/// 737,280 bytes/s (§3.1.4 "Data rate"). The verdict of #38 is measured against this.
inline constexpr std::uint64_t kTargetBytesPerSecond = TILEMAP_BANK_SIZE * kTickRateHz;

/// Store width used to copy the tilemap over the bridge.
enum class WriteWidth : std::uint8_t {
    k8,   ///< one byte per store: the path RegisterSpace::writeBlock8() takes today
    k32,  ///< four bytes per store: a faster path #34 may choose for writeBlock8()
};

/// Command-line options of bridge_benchmark.
struct Options {
    std::uint32_t passes = 600;  ///< full-bank writes per width (600 = 10 s of ticks)
    std::vector<WriteWidth> widths{WriteWidth::k8, WriteWidth::k32};
    std::uint8_t value = 0;  ///< tile index written to every cell (0 = the reset value)
    bool help = false;
};

/// Parses `--passes N` (1..100000), `--width 8|32|both`, `--value N` (0..255) and `--help`.
/// Returns std::nullopt and sets `error` on a bad argument.
std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error);

/// Usage text for --help and for a bad argument.
std::string usage();

/// Byte offset of the back bank, from a read of the Swap Request / Front Bank register:
/// front bank 0 -> bank 1, front bank 1 -> bank 0 (§3.1.4, SR-HAL-06).
std::uint32_t backBankOffset(std::uint32_t swapReqFrontBank) noexcept;

/// Writes `value` to every cell of one tilemap bank (TILEMAP_BANK_SIZE bytes) with byte stores.
void writeBank8(volatile std::uint8_t* bank, std::uint8_t value) noexcept;

/// Writes `value` to every cell of one tilemap bank with 32-bit stores (TILEMAP_BANK_SIZE / 4
/// words, each holding four cells).
void writeBank32(volatile std::uint32_t* bank, std::uint8_t value) noexcept;

/// Bytes per second for `bytes` written in `nanoseconds`; 0 when `nanoseconds` is 0.
std::uint64_t bytesPerSecond(std::uint64_t bytes, std::uint64_t nanoseconds) noexcept;

/// True when `measured` meets the §3.1.4 worst-case data rate (kTargetBytesPerSecond).
bool meetsDataRate(std::uint64_t measuredBytesPerSecond) noexcept;

}  // namespace arcade::hal::bench
