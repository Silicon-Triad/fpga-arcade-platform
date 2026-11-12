#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <vector>

#include "hal/register_map.hpp"
#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"

namespace arcade::hal::test {

/// One recorded bridge access.
struct Access {
    enum class Kind : std::uint8_t { kRead8, kWrite8, kRead32, kWrite32 } kind;
    std::uint32_t offset;
    std::uint32_t value;
};

/// Fake clock: time moves only when a test (or a mock hook) advances it.
class FakeClock final : public MonotonicClock {
  public:
    std::chrono::nanoseconds now() noexcept override { return t; }
    std::chrono::nanoseconds t{0};
};

/// In-memory bridge window that records every access in order. Front Bank and Status reads
/// are served from fields; onStatusRead lets a test change state per poll (e.g. advance
/// the fake clock or clear swap pending).
class MockRegisterSpace final : public RegisterSpace {
  public:
    std::uint8_t read8(std::uint32_t offset) noexcept override {
        countIfReleased();
        log.push_back({Access::Kind::kRead8, offset, mem8[offset]});
        return mem8[offset];
    }
    void write8(std::uint32_t offset, std::uint8_t value) noexcept override {
        countIfReleased();
        log.push_back({Access::Kind::kWrite8, offset, value});
        mem8[offset] = value;
    }
    std::uint32_t read32(std::uint32_t offset) noexcept override {
        countIfReleased();
        std::uint32_t v = 0;
        if (offset == kOffsetControlReg) {
            v = control;
        } else if (offset == kOffsetSwapReqFrontBank) {
            v = front << kBitFrontBank;
        } else if (offset == kOffsetStatusReg) {
            ++statusReads;
            if (onStatusRead) {
                onStatusRead(statusReads);
            }
            v = status;
        } else if (offset == kOffsetFrameCounter) {
            v = frameCounter;
        } else if (offset == kOffsetIdVersion) {
            v = idVersion;
        }
        log.push_back({Access::Kind::kRead32, offset, v});
        return v;
    }
    void write32(std::uint32_t offset, std::uint32_t value) noexcept override {
        countIfReleased();
        log.push_back({Access::Kind::kWrite32, offset, value});
        if (offset == kOffsetControlReg) {
            control = value;
        }
    }
    /// SR-HAL-07 release hook (#34 implements the real munmap); records the call.
    void release() noexcept override { ++releaseCalls; }

    void countIfReleased() noexcept {
        if (releaseCalls > 0) {
            ++accessesAfterRelease;
        }
    }

    std::array<std::uint8_t, kOffsetTilePatternRam + TILE_PATTERN_RAM_SIZE>
        mem8{};  // banks, patterns
    std::vector<Access> log;
    std::uint32_t front = 0;
    std::uint32_t status = 0;
    std::uint32_t frameCounter = 0;
    std::uint32_t idVersion = 0;  // ID/Version (SR-HAL-09), served by read32
    std::uint32_t control = 0;    // Control Register (read-modify-write, SR-HAL-13)
    int releaseCalls = 0;
    int accessesAfterRelease = 0;
    int statusReads = 0;
    std::function<void(int)> onStatusRead;
};

}  // namespace arcade::hal::test
