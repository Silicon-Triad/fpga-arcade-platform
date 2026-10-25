/// @file
/// SDL2 backend of the Arcade API for the PC simulation (SR-FW-05 to SR-FW-07, QA-03).
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "arcade/engine_api.hpp"
#include "arcade/types.hpp"
#include "backend_sdl/key_action_lookup.hpp"
#include "backend_sdl/key_codes.hpp"
#include "backend_sdl/text_layout.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace arcade {

/// Window width in pixels (SR-FW-06: one 1024 x 768 window).
constexpr int kWindowWidth = 1024;
/// Window height in pixels (SR-FW-06: one 1024 x 768 window).
constexpr int kWindowHeight = 768;
static_assert(kWindowWidth == kGridCols * kTilePx, "SR-FW-06: 128 tiles of 8 px");
static_assert(kWindowHeight == kGridRows * kTilePx, "SR-FW-06: 96 tiles of 8 px");

/// EngineApi implemented with SDL2: draws the 128 x 96 tilemap in one 1024 x 768 window and
/// reads the PC keyboard, mapped to actions through an injected KeyActionLookup (SR-FW-05,
/// SR-FW-06). Sound calls are accepted and silent (SR-FW-07). No call throws (R-4.1.5).
///
/// One instance at a time: it owns SDL initialisation and quits SDL when destroyed.
class SdlBackend final : public EngineApi {
  public:
    /// Opens the window and initialises SDL. `lookup` (and `textLayout`, if given) must
    /// outlive the backend. Returns nullptr if SDL fails (SR-FW-06).
    static std::unique_ptr<SdlBackend> open(const KeyActionLookup& lookup,
                                            const TextLayout* textLayout = nullptr) noexcept;

    ~SdlBackend() override;
    SdlBackend(const SdlBackend&) = delete;
    SdlBackend& operator=(const SdlBackend&) = delete;
    SdlBackend(SdlBackend&&) = delete;
    SdlBackend& operator=(SdlBackend&&) = delete;

    /// Writes one shadow-tilemap cell (SR-HAL-03 semantics, SR-FW-06).
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override;
    /// Fills a rectangle of the shadow tilemap (App. A Tilemap drawing).
    ApiStatus fillRect(int col, int row, int width, int height, TileIndex tile) noexcept override;
    /// Draws text through the TextLayout hook; BackendError and nothing drawn while no hook is
    /// set (SR-FW-12, issue #9).
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override;
    /// Sets every shadow-tilemap cell to `tile` (App. A Tilemap drawing).
    void clear(TileIndex tile) noexcept override;
    /// Draws the tilemap into the window and returns at the next 1/TICK_RATE frame boundary,
    /// timed by the CPU clock rather than the monitor's vsync (SR-FW-02, SR-FW-06).
    ApiStatus present() noexcept override;
    /// Drains SDL events without blocking and returns this tick's actions (SR-FW-06,
    /// SR-INP-02).
    InputFrame pollInput() noexcept override;
    /// Accepted without error; no sound is produced (SR-FW-07).
    void playSound(EffectId effectId) noexcept override;
    /// Accepted without error; the mute state is kept (SR-FW-07, SR-FW-11).
    void setMute(bool on) noexcept override;
    /// Stores the tile patterns and palette used by present() (SR-FW-01, TBL-TilePattern,
    /// TBL-Palette).
    ApiStatus loadTiles(const TileSet& patterns, const Palette& palette) noexcept override;
    /// Frame boundaries passed since the window opened, as of the last present() (SR-FW-02).
    std::uint32_t frameCount() const noexcept override { return frameCount_; }
    /// Sets the context passed to the key lookup (SR-INP-09); InvalidArgument for Global.
    ApiStatus setInputContext(InputContext context) noexcept override;

    /// True once the window was closed (SDL_QUIT seen by pollInput()).
    bool quitRequested() const noexcept { return quitRequested_; }
    /// True while muted (SR-FW-11).
    bool muted() const noexcept { return muted_; }
    /// Colour 0x00RRGGBB of window pixel (x, y) drawn by the last present(), or 0 outside the
    /// window. For tests (SR-FW-06).
    std::uint32_t pixelAt(int x, int y) const noexcept;
    /// True if the key with Linux code `keyCode` is held (App. A key_state, SR-INP-08).
    bool isKeyHeld(std::uint16_t keyCode) const noexcept {
        return keyCode < linux_key::kCount && keyHeld_[keyCode];
    }

  private:
    /// Events held between SDL's queue and pollInput(). DESIGN CHOICE, NOT AN SRS VALUE: 8
    /// ticks' worth of kMaxEventsPerTick. When full, further events stay in SDL's own queue.
    static constexpr std::size_t kQueueCapacity = 8 * kMaxEventsPerTick;

    SdlBackend(const KeyActionLookup& lookup, const TextLayout* textLayout) noexcept;
    bool init() noexcept;
    void handleKey(bool down, int scancode) noexcept;

    const KeyActionLookup& lookup_;
    const TextLayout* textLayout_;
    bool sdlInitialised_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    /// Window pixels, ARGB8888, row-major.
    using Pixels =
        std::array<std::uint32_t, static_cast<std::size_t>(kWindowWidth) * kWindowHeight>;
    std::unique_ptr<Pixels> pixels_;

    Tilemap shadow_{};
    /// Every tile expanded to ARGB8888 pixels by loadTiles(), row-major, so present() copies
    /// whole tile rows (TBL-TilePattern, TBL-Palette).
    using TilePixels = std::array<std::uint32_t, static_cast<std::size_t>(kTilePx) * kTilePx>;
    std::array<TilePixels, kTileCount> tilePixels_{};

    std::chrono::steady_clock::time_point start_{};
    std::uint32_t frameCount_ = 0;

    InputContext context_ = InputContext::Menu;
    std::array<bool, linux_key::kCount> keyHeld_{};
    std::array<InputEvent, kQueueCapacity> queue_{};
    std::size_t queueHead_ = 0;
    std::size_t queueSize_ = 0;
    bool quitRequested_ = false;

    bool muted_ = false;
};

}  // namespace arcade
