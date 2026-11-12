/// @file
/// FPGA backend of the Arcade API for the DE1-SoC board, built on the HAL (SR-FW-05, QA-03).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "arcade/engine_api.hpp"
#include "arcade/types.hpp"
#include "hal/hal_lifecycle.hpp"
#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"
#include "input/input_manager.hpp"
#include "input/key_bindings.hpp"

namespace arcade {

/// Hook for the sound driver of #44 (SR-HAL-10 to SR-HAL-12). FpgaBackend forwards every
/// playSound() and setMute() call to it and calls tick() once per present(), so the driver can
/// start each following note in the tick in which the previous one ends (SR-HAL-11). Without a
/// driver every sound call is accepted and silent (SR-FW-07). No call throws (R-4.1.5).
class FpgaSound {
  public:
    virtual ~FpgaSound() = default;
    /// SR-HAL-11: start effect `effectId`, replacing any effect still playing.
    virtual void play(EffectId effectId) noexcept = 0;
    /// SR-HAL-12: set the mute state.
    virtual void setMute(bool on) noexcept = 0;
    /// SR-HAL-11: called once per game tick, from present().
    virtual void tick() noexcept = 0;
};

/// EngineApi on the HAL: drawing goes to the shadow tilemap (SR-HAL-03, SR-HAL-04), present()
/// to TilemapHal::present() (SR-HAL-08), loadTiles() to HalLifecycle::loadAssets() (SR-HAL-13),
/// and input comes from the Input Manager through TBL-KeyBinding (SR-INP-09 to SR-INP-11). It
/// returns the same codes as SdlBackend for the same calls (SR-FW-05). Bridge access happens
/// only in present(), loadTiles(), frameCount() and shutdown(). Fixed-size storage, no heap use
/// per call (R-4.1.6). No call throws (R-4.1.5).
class FpgaBackend final : public EngineApi {
  public:
    /// Binds to the bridge, clock, Input Manager and bindings, which must all outlive the
    /// backend. `input` must already be started. `sound` may be nullptr (no driver yet, #44).
    /// No bridge access.
    FpgaBackend(hal::RegisterSpace& regs, hal::MonotonicClock& clock, InputManager& input,
                const KeyBindings& bindings, FpgaSound* sound = nullptr) noexcept;
    /// Calls shutdown() (SR-HAL-07): Display Enable stays set.
    ~FpgaBackend() override;
    FpgaBackend(const FpgaBackend&) = delete;
    FpgaBackend& operator=(const FpgaBackend&) = delete;
    FpgaBackend(FpgaBackend&&) = delete;
    FpgaBackend& operator=(FpgaBackend&&) = delete;

    /// SR-HAL-03: one shadow cell; OutOfRange outside the grid. No bridge access.
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override;
    /// SR-HAL-04: a shadow rectangle; OutOfRange if any part is outside the grid or the width
    /// or height is not positive, as in SdlBackend. No bridge access.
    ApiStatus fillRect(int col, int row, int width, int height, TileIndex tile) noexcept override;
    /// SR-FW-12: the shared font layout (assets::textToTiles()), the same tiles as SdlBackend.
    /// No bridge access.
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override;
    /// Sets every shadow cell to `tile` (App. A Tilemap drawing). No bridge access.
    void clear(TileIndex tile) noexcept override;
    /// SR-HAL-08: copies changed cells to the back bank, requests the swap and waits for it
    /// (SR-HAL-05), which locks the loop to the display (SR-FW-02). BackendError on a swap
    /// timeout or after shutdown().
    ApiStatus present() noexcept override;
    /// SR-INP-02: never blocks. Reads the Input Manager, maps keys through the current context
    /// then Global (SR-INP-09 to SR-INP-11) and reports KeyboardLost and KeyboardFound
    /// (SR-INP-14, SR-INP-15).
    InputFrame pollInput() noexcept override;
    /// SR-FW-07: always accepted; forwarded to the sound driver if there is one (#44).
    void playSound(EffectId effectId) noexcept override;
    /// SR-FW-07, SR-FW-11: always accepted; the state is kept and forwarded to the driver.
    void setMute(bool on) noexcept override;
    /// SR-HAL-13: patterns, then palette, then Display Enable. InvalidArgument (and Display
    /// Enable not set) for a palette entry above 0x00RRGGBB; BackendError after shutdown().
    ApiStatus loadTiles(const TileSet& patterns, const Palette& palette) noexcept override;
    /// SR-HAL-06: reads the Frame Counter register; after shutdown(), the last value read.
    std::uint32_t frameCount() const noexcept override;
    /// SR-INP-09: InvalidArgument for Global, which is never current.
    ApiStatus setInputContext(InputContext context) noexcept override;

    /// SR-HAL-07: releases the bridge once without writing any register, so the last frame
    /// stays on screen. Later calls do nothing; the destructor calls it too.
    void shutdown() noexcept;

    /// True while muted (SR-FW-11).
    bool muted() const noexcept { return muted_; }
    /// Shadow cell (col, row), or 0 outside the grid. For tests; no bridge access.
    TileIndex shadowTile(int col, int row) const noexcept { return tilemap_.shadowTile(col, row); }
    /// Key events dropped because the queue was full (see kQueueCapacity). For tests and
    /// diagnostics; 0 in normal use.
    std::size_t droppedEvents() const noexcept { return dropped_; }

  private:
    /// Events held between the Input Manager and pollInput(). DESIGN CHOICE, NOT AN SRS VALUE:
    /// 8 ticks' worth of kMaxEventsPerTick, as in SdlBackend. The Input Manager drains the
    /// kernel queue in one call (SR-INP-04), so the backend reads it only when this queue is
    /// empty and the rest stays in the kernel queue meanwhile. The last kReservedSlots slots
    /// are kept for KeyboardLost and KeyboardFound, so neither is ever dropped.
    static constexpr std::size_t kQueueCapacity = 8 * kMaxEventsPerTick;
    static constexpr std::size_t kReservedSlots = 2;
    /// Per-key arrays: one entry per Linux key code (`KEY_CNT`, App. A key_state).
    static constexpr std::size_t kKeyCount = KEY_CNT;

    void onKeyEvent(const KeyEvent& event) noexcept;
    void push(const InputEvent& event) noexcept;

    /// Mutable: frameCount() is const in the API but reads the Frame Counter register.
    mutable hal::TilemapHal tilemap_;
    hal::HalLifecycle lifecycle_;
    InputManager& input_;
    const KeyBindings& bindings_;
    FpgaSound* sound_;

    InputContext context_ = InputContext::Menu;
    /// Event of each held key at its KeyDown, so its KeyUp reports the same action even if the
    /// context changed while it was held (as SdlBackend does).
    std::array<std::optional<InputEvent>, kKeyCount> pressEvent_{};
    std::array<InputEvent, kQueueCapacity> queue_{};
    std::size_t queueHead_ = 0;
    std::size_t queueSize_ = 0;
    std::size_t dropped_ = 0;

    bool muted_ = false;
    mutable std::uint32_t lastFrameCount_ = 0;
};

}  // namespace arcade
