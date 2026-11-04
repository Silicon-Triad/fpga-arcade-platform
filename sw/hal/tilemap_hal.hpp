#pragma once

#include <array>
#include <bitset>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include "hal/register_map.hpp"
#include "hal/register_space.hpp"

namespace arcade::hal {

/// VSYNC_TIMEOUT (App. A): present() swap-pending poll timeout, 100 ms = 6 frame periods.
inline constexpr std::chrono::milliseconds VSYNC_TIMEOUT{100};  // SR-HAL-05

/// Distinct HAL result codes (SR-HAL-02 style, R-4.1.5). No HAL call throws.
enum class HalStatus : std::uint8_t {
    kOk,           ///< The call succeeded.
    kOutOfRange,   ///< A cell coordinate or rectangle lies outside the 128x96 grid.
    kSwapTimeout,  ///< Swap pending did not clear within VSYNC_TIMEOUT (SR-HAL-05).
};

/// Monotonic time source for the SR-HAL-05 poll, injected so tests need not sleep.
class MonotonicClock {
  public:
    virtual ~MonotonicClock() = default;
    /// Current time since an arbitrary fixed epoch; never goes backwards.
    virtual std::chrono::nanoseconds now() noexcept = 0;
};

/// MonotonicClock backed by std::chrono::steady_clock (used on the board).
class SteadyClock final : public MonotonicClock {
  public:
    /// Return steady_clock::now() since its epoch.
    std::chrono::nanoseconds now() noexcept override;
};

/// Shadow tilemap, present() and frame state of the HAL (SR-HAL-03, 04, 05, 06, 08).
/// Drawing calls touch only process RAM; present() is the only call that writes the
/// tilemap banks over the bridge. Fixed-size storage, no heap use per call (R-4.1.6).
class TilemapHal {
  public:
    /// Bind to a register space and clock; both must outlive this object. No bridge access.
    TilemapHal(RegisterSpace& regs, MonotonicClock& clock) noexcept;

    /// SR-HAL-03: write one tile index at (col, row) of the shadow tilemap; no bridge access.
    /// Returns kOutOfRange if the cell lies outside GRID_COLS x GRID_ROWS.
    HalStatus setTile(int col, int row, std::uint8_t tile) noexcept;

    /// SR-HAL-04: fill a width x height rectangle (up to 128x96) of the shadow tilemap in one
    /// call; no bridge access. Returns kOutOfRange, writing nothing, if any part of the
    /// rectangle lies outside the grid or width/height is negative. An empty rectangle
    /// (width or height 0) inside the grid is kOk and writes nothing.
    HalStatus fillRect(int col, int row, int width, int height, std::uint8_t tile) noexcept;

    /// Read one shadow cell (no bridge access); 0 if out of range. For callers and tests.
    std::uint8_t shadowTile(int col, int row) const noexcept;

    /// SR-HAL-08: (1) read Front Bank and copy to the back bank every cell changed since
    /// that bank was last written (the whole bank on the first call); (2) write Swap
    /// Request; (3) return the result of waitSwapComplete() (SR-HAL-05).
    HalStatus present() noexcept;

    /// SR-HAL-05: blocking poll of Status swap-pending. Returns kOk as soon as it reads
    /// clear, or kSwapTimeout once more than VSYNC_TIMEOUT has passed without it clearing.
    HalStatus waitSwapComplete() noexcept;

    /// SR-HAL-06: non-blocking read of the Front Bank bit (0 or 1).
    unsigned frontBank() noexcept;

    /// SR-HAL-06: non-blocking read of the Frame Counter register.
    std::uint32_t frameCounter() noexcept;

  private:
    static_assert(GRID_COLS * GRID_ROWS == TILEMAP_BANK_SIZE, "TBL-Tilemap: 1 byte per cell");
    static constexpr std::size_t kBankCount = 2;  // SR-REN-01: bank 0 and bank 1

    void markDirty(std::size_t index) noexcept;

    RegisterSpace& regs_;
    MonotonicClock& clock_;
    std::array<std::uint8_t, TILEMAP_BANK_SIZE> shadow_{};  // App. A shadowTilemap
    /// SR-HAL-08: per bank, the cells changed since that bank was last written.
    std::array<std::bitset<TILEMAP_BANK_SIZE>, kBankCount> stale_{};
};

}  // namespace arcade::hal
