#pragma once

#include <cstddef>
#include <cstdint>

namespace arcade::hal {

/// Register-access seam between the HAL and the lightweight HPS-to-FPGA bridge window
/// (§3.1.4). Every HAL bridge access goes through this interface, addressed by the byte
/// offset inside the window (the kOffset* constants of register_map.hpp).
///
/// Implementations: the /dev/mem mmap window (#34, SR-HAL-01) and the in-memory mock in
/// sw/tests/hal/. The start-up load and shutdown (#36) and the sound registers (#44) use the
/// same interface unchanged. No call throws (R-4.1.5).
class RegisterSpace {
  public:
    virtual ~RegisterSpace() = default;

    /// Read one byte at `offset` (tilemap and tile-pattern regions, §3.1.4).
    virtual std::uint8_t read8(std::uint32_t offset) noexcept = 0;
    /// Write one byte at `offset` (tilemap and tile-pattern regions, §3.1.4).
    virtual void write8(std::uint32_t offset, std::uint8_t value) noexcept = 0;
    /// Read one 32-bit register at `offset` (control, status and counter registers, §3.1.4).
    virtual std::uint32_t read32(std::uint32_t offset) noexcept = 0;
    /// Write one 32-bit register at `offset` (control and request registers, §3.1.4).
    virtual void write32(std::uint32_t offset, std::uint32_t value) noexcept = 0;

    /// Write `count` consecutive bytes starting at `offset`. The default calls write8() once
    /// per byte; an implementation may override it with a faster copy.
    virtual void writeBlock8(std::uint32_t offset, const std::uint8_t* data,
                             std::size_t count) noexcept {
        for (std::size_t i = 0; i < count; ++i) {
            write8(offset + static_cast<std::uint32_t>(i), data[i]);
        }
    }

    /// SR-HAL-07: release the bridge resources this space holds (#34: munmap the window and
    /// close /dev/mem). Called once, by HalLifecycle::shutdown(). After it returns, the space
    /// performs no bridge access: later reads return 0 and later writes are dropped. It never
    /// writes any register itself, so Display Enable stays set. The default does nothing.
    virtual void release() noexcept {}
};

}  // namespace arcade::hal
