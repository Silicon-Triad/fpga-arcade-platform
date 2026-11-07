#include "hal/bridge_benchmark.hpp"

#include <cstddef>

#include "hal/register_map.hpp"

namespace arcade::hal {

BridgeBenchResult benchmarkTilemapWrites(RegisterSpace& regs, MonotonicClock& clock,
                                         std::uint32_t passes) noexcept {
    BridgeBenchResult result;
    // SR-HAL-06: read Front Bank once; write only the other (back) bank so nothing on screen
    // changes (SR-REN-01, §3.1.4 register map).
    const unsigned front = (regs.read32(kOffsetSwapReqFrontBank) >> kBitFrontBank) & 1u;
    result.backBankOffset = (front == 0u) ? kOffsetTilemapBank1 : kOffsetTilemapBank0;
    if (passes == 0) {
        return result;
    }

    const auto start = clock.now();
    for (std::uint32_t p = 0; p < passes; ++p) {
        // §3.1.4 data rate: one full bank per pass, byte writes as present() does (SR-HAL-08).
        // The value changes per pass so no write could be skipped as a repeat.
        for (std::size_t i = 0; i < TILEMAP_BANK_SIZE; ++i) {
            regs.write8(result.backBankOffset + static_cast<std::uint32_t>(i),
                        static_cast<std::uint8_t>(i + p));
        }
    }
    result.elapsed = clock.now() - start;
    result.bytesWritten = static_cast<std::uint64_t>(passes) * TILEMAP_BANK_SIZE;

    const auto ns = static_cast<std::uint64_t>(result.elapsed.count());
    if (ns > 0) {
        // Split to avoid overflow of bytesWritten * 1e9 for long runs.
        constexpr std::uint64_t kNsPerS = 1000000000ULL;
        result.bytesPerSecond =
            (result.bytesWritten / ns) * kNsPerS + ((result.bytesWritten % ns) * kNsPerS) / ns;
    }
    result.meetsWorstCase = result.bytesPerSecond >= kWorstCaseTilemapBytesPerSecond;
    return result;
}

}  // namespace arcade::hal
