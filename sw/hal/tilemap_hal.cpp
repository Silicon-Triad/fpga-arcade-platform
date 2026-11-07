#include "hal/tilemap_hal.hpp"

namespace arcade::hal {
namespace {

// TBL-Tilemap: cell (col, row) is byte row * GRID_COLS + col of a bank (same as
// hw/rtl/tilemap_crop.sv).
constexpr std::size_t cellIndex(int col, int row) noexcept {
    return static_cast<std::size_t>(row) * GRID_COLS + static_cast<std::size_t>(col);
}

constexpr bool inGrid(int col, int row) noexcept {
    return col >= 0 && row >= 0 && static_cast<std::size_t>(col) < GRID_COLS &&
           static_cast<std::size_t>(row) < GRID_ROWS;
}

constexpr std::uint32_t bankOffset(unsigned bank) noexcept {
    return bank == 0 ? kOffsetTilemapBank0 : kOffsetTilemapBank1;  // §3.1.4
}

}  // namespace

std::chrono::nanoseconds SteadyClock::now() noexcept {
    return std::chrono::steady_clock::now().time_since_epoch();
}

TilemapHal::TilemapHal(RegisterSpace& regs, MonotonicClock& clock) noexcept
    : regs_(regs), clock_(clock) {
    // SR-HAL-08: the banks' contents after reset are not known to match the shadow, so the
    // first present() to each bank writes every cell.
    for (auto& bank : stale_) {
        bank.set();
    }
}

void TilemapHal::markDirty(std::size_t index) noexcept {
    for (auto& bank : stale_) {
        bank.set(index);
    }
}

HalStatus TilemapHal::setTile(int col, int row, std::uint8_t tile) noexcept {
    // SR-HAL-03: shadow tilemap only; no bridge access.
    if (!inGrid(col, row)) {
        return HalStatus::kOutOfRange;
    }
    const std::size_t i = cellIndex(col, row);
    if (shadow_[i] != tile) {
        shadow_[i] = tile;
        markDirty(i);
    }
    return HalStatus::kOk;
}

HalStatus TilemapHal::fillRect(int col, int row, int width, int height,
                               std::uint8_t tile) noexcept {
    // SR-HAL-04: up to 128x96 cells in one call, shadow tilemap only; no bridge access.
    if (width < 0 || height < 0 || !inGrid(col, row) ||
        static_cast<std::size_t>(col) + static_cast<std::size_t>(width) > GRID_COLS ||
        static_cast<std::size_t>(row) + static_cast<std::size_t>(height) > GRID_ROWS) {
        return HalStatus::kOutOfRange;
    }
    for (int y = row; y < row + height; ++y) {
        for (int x = col; x < col + width; ++x) {
            const std::size_t i = cellIndex(x, y);
            if (shadow_[i] != tile) {
                shadow_[i] = tile;
                markDirty(i);
            }
        }
    }
    return HalStatus::kOk;
}

std::uint8_t TilemapHal::shadowTile(int col, int row) const noexcept {
    return inGrid(col, row) ? shadow_[cellIndex(col, row)] : std::uint8_t{0};
}

HalStatus TilemapHal::present() noexcept {
    // SR-HAL-08: never write a bank while an earlier swap is still pending (e.g. after a
    // timed-out present()): the pending swap could put the bank on screen mid-copy. Wait for
    // it first (SR-HAL-05); if it still has not cleared, write nothing and report timeout.
    if ((regs_.read32(kOffsetStatusReg) & (1u << kBitStatusSwapPending)) != 0u) {
        const HalStatus pending = waitSwapComplete();
        if (pending != HalStatus::kOk) {
            return pending;
        }
    }
    // SR-HAL-08 (1): the back bank is the one not in front; it is two frames behind the
    // shadow, so copy every cell changed since THIS bank was last written.
    const unsigned back = frontBank() ^ 1u;
    auto& stale = stale_[back];
    const std::uint32_t base = bankOffset(back);
    for (std::size_t i = 0; i < TILEMAP_BANK_SIZE; ++i) {
        if (stale.test(i)) {
            regs_.write8(base + static_cast<std::uint32_t>(i), shadow_[i]);
        }
    }
    stale.reset();
    // SR-HAL-08 (2): request the swap; hardware applies it at the next vblank (SR-REN-03).
    regs_.write32(kOffsetSwapReqFrontBank, 1u << kBitSwapRequest);
    // SR-HAL-08 (3): poll until the swap is confirmed or times out (SR-HAL-05).
    return waitSwapComplete();
}

HalStatus TilemapHal::waitSwapComplete() noexcept {
    // SR-HAL-05: poll Status swap-pending; no vsync wait before it.
    const auto start = clock_.now();
    for (;;) {
        const std::uint32_t status = regs_.read32(kOffsetStatusReg);
        if ((status & (1u << kBitStatusSwapPending)) == 0u) {
            return HalStatus::kOk;
        }
        if (clock_.now() - start > VSYNC_TIMEOUT) {
            return HalStatus::kSwapTimeout;
        }
    }
}

unsigned TilemapHal::frontBank() noexcept {
    // SR-HAL-06: one register read, no wait.
    return (regs_.read32(kOffsetSwapReqFrontBank) >> kBitFrontBank) & 1u;
}

std::uint32_t TilemapHal::frameCounter() noexcept {
    // SR-HAL-06: one register read, no wait.
    return regs_.read32(kOffsetFrameCounter);
}

}  // namespace arcade::hal
