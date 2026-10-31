// SDL2 backend of the Arcade API (SR-FW-05 to SR-FW-07).
#include "backend_sdl/sdl_backend.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <new>
#include <thread>

#include "arcade/tables.hpp"

namespace arcade {
namespace {

/// One display frame: 1/TICK_RATE s (SR-FW-02).
using Frame = std::chrono::duration<std::int64_t, std::ratio<1, TICK_RATE>>;

/// Opaque alpha for ARGB8888 pixels; TBL-Palette leaves the top byte 0x00.
constexpr std::uint32_t kOpaque = 0xFF000000U;
/// Mask of the 0x00RRGGBB colour bits (TBL-Palette rgb).
constexpr std::uint32_t kRgbMask = 0x00FFFFFFU;
/// Pixels per pattern byte and nibble mask (TBL-TilePattern: 4 bits/pixel, 2 pixels/byte).
constexpr std::size_t kPixelsPerByte = 2;
constexpr unsigned kNibbleMask = 0x0FU;
constexpr unsigned kNibbleBits = 4;
/// Grid and tile sizes as std::size_t, for indexing (SR-FW-06).
constexpr auto kCols = static_cast<std::size_t>(kGridCols);
constexpr auto kRows = static_cast<std::size_t>(kGridRows);
constexpr auto kTile = static_cast<std::size_t>(kTilePx);
constexpr auto kWidth = static_cast<std::size_t>(kWindowWidth);

bool inGrid(int col, int row) { return col >= 0 && col < kGridCols && row >= 0 && row < kGridRows; }

}  // namespace

std::unique_ptr<SdlBackend> SdlBackend::open(const KeyActionLookup& lookup,
                                             const TextLayout* textLayout) noexcept {
    std::unique_ptr<SdlBackend> backend(new (std::nothrow) SdlBackend(lookup, textLayout));
    if (!backend || !backend->init()) {
        return nullptr;
    }
    return backend;
}

SdlBackend::SdlBackend(const KeyActionLookup& lookup, const TextLayout* textLayout) noexcept
    : lookup_(lookup), textLayout_(textLayout) {}

bool SdlBackend::init() noexcept {
    pixels_.reset(new (std::nothrow) Pixels());
    if (!pixels_) {
        return false;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        return false;
    }
    sdlInitialised_ = true;
    // SR-FW-06: a single 1024 x 768 window.
    window_ = SDL_CreateWindow("FPGA Arcade", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               kWindowWidth, kWindowHeight, 0);
    if (window_ == nullptr) {
        return false;
    }
    // No SDL_RENDERER_PRESENTVSYNC: frames are paced by the CPU clock (see present()).
    renderer_ = SDL_CreateRenderer(window_, -1, 0);
    if (renderer_ == nullptr) {
        return false;
    }
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                 kWindowWidth, kWindowHeight);
    if (texture_ == nullptr) {
        return false;
    }
    start_ = std::chrono::steady_clock::now();
    return true;
}

SdlBackend::~SdlBackend() {
    if (texture_ != nullptr) {
        SDL_DestroyTexture(texture_);
    }
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    if (sdlInitialised_) {
        SDL_Quit();
    }
}

ApiStatus SdlBackend::setTile(int col, int row, TileIndex tile) noexcept {
    if (!inGrid(col, row)) {
        return ApiStatus::OutOfRange;
    }
    shadow_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = tile;
    return ApiStatus::Ok;
}

ApiStatus SdlBackend::fillRect(int col, int row, int width, int height, TileIndex tile) noexcept {
    // Checked without computing col + width, which could overflow.
    if (width <= 0 || height <= 0 || !inGrid(col, row) || width > kGridCols - col ||
        height > kGridRows - row) {
        return ApiStatus::OutOfRange;
    }
    for (int r = row; r < row + height; ++r) {
        for (int c = col; c < col + width; ++c) {
            shadow_[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = tile;
        }
    }
    return ApiStatus::Ok;
}

ApiStatus SdlBackend::drawText(int col, int row, std::string_view text, TextScale scale) noexcept {
    // SR-FW-12: font tiles and layout come from issue #9; without the hook nothing is drawn.
    if (textLayout_ == nullptr) {
        return ApiStatus::BackendError;
    }
    return textLayout_->layout(col, row, text, scale, shadow_);
}

void SdlBackend::clear(TileIndex tile) noexcept {
    for (auto& line : shadow_) {
        line.fill(tile);
    }
}

ApiStatus SdlBackend::present() noexcept {
    // SR-FW-06: copy each cell's expanded 8 x 8 tile into the window pixels.
    for (std::size_t row = 0; row < kRows; ++row) {
        for (std::size_t col = 0; col < kCols; ++col) {
            const TilePixels& tile = tilePixels_[shadow_[row][col]];
            for (std::size_t py = 0; py < kTile; ++py) {
                const auto* src = tile.data() + py * kTile;
                std::copy(src, src + kTile,
                          pixels_->data() + (row * kTile + py) * kWidth + col * kTile);
            }
        }
    }
    const int pitch = kWindowWidth * static_cast<int>(sizeof(std::uint32_t));
    if (SDL_UpdateTexture(texture_, nullptr, pixels_->data(), pitch) != 0 ||
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr) != 0) {
        return ApiStatus::BackendError;
    }
    SDL_RenderPresent(renderer_);

    // SR-FW-02: return at the next 1/TICK_RATE boundary, timed by the CPU clock because the
    // monitor's refresh rate may not be 60 Hz.
    const auto elapsed =
        std::chrono::duration_cast<Frame>(std::chrono::steady_clock::now() - start_);
    std::this_thread::sleep_until(start_ + elapsed + Frame(1));
    frameCount_ = static_cast<std::uint32_t>(
        std::chrono::duration_cast<Frame>(std::chrono::steady_clock::now() - start_).count());
    return ApiStatus::Ok;
}

void SdlBackend::handleKey(bool down, int scancode) noexcept {
    const auto code = linuxKeyCode(static_cast<SDL_Scancode>(scancode));
    if (!code) {
        return;
    }
    keyHeld_[*code] = down;  // App. A key_state, SR-INP-08
    // KeyDown maps through the current context; KeyUp reuses its KeyDown's mapping.
    std::optional<KeyMapping> mapping;
    if (down) {
        mapping = lookup_.lookup(context_, *code);
        pressMapping_[*code] = mapping;
    } else {
        mapping = pressMapping_[*code];
        pressMapping_[*code].reset();
    }
    if (!mapping) {
        return;  // unbound keys produce no event (SR-INP-09)
    }
    InputEvent ev;
    ev.type = down ? InputEventType::KeyDown : InputEventType::KeyUp;
    ev.action = mapping->action;
    ev.ch = mapping->action == Action::Char ? mapping->ch : '\0';
    queue_[(queueHead_ + queueSize_) % kQueueCapacity] = ev;
    ++queueSize_;
}

InputFrame SdlBackend::pollInput() noexcept {
    // SR-INP-02: SDL_PollEvent never blocks. Stop while the queue is full so the rest stay in
    // SDL's queue and none is dropped.
    SDL_Event e;
    while (queueSize_ < kQueueCapacity && SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            quitRequested_ = true;
        } else if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && e.key.repeat == 0) {
            // SR-INP-07: auto-repeat is discarded.
            handleKey(e.type == SDL_KEYDOWN, e.key.keysym.scancode);
        }
    }

    InputFrame frame;
    while (frame.count < kMaxEventsPerTick && queueSize_ > 0) {
        frame.events[frame.count++] = queue_[queueHead_];
        queueHead_ = (queueHead_ + 1) % kQueueCapacity;
        --queueSize_;
    }
    // App. A Held-key state: held keys through the current context's bindings (SR-INP-08,
    // SR-INP-09).
    for (std::uint16_t code = 0; code < linux_key::kCount; ++code) {
        if (keyHeld_[code]) {
            if (const auto mapping = lookup_.lookup(context_, code)) {
                frame.held[static_cast<std::size_t>(mapping->action)] = true;
            }
        }
    }
    return frame;
}

void SdlBackend::playSound(EffectId /*effectId*/) noexcept {
    // SR-FW-07: every call is accepted; audible sound on the PC is optional and not produced.
}

void SdlBackend::setMute(bool on) noexcept {
    muted_ = on;  // SR-FW-07, SR-FW-11
}

ApiStatus SdlBackend::loadTiles(const TileSet& patterns, const Palette& palette) noexcept {
    // SR-FW-06: expand every pattern through the palette once, not on every present().
    for (std::size_t t = 0; t < kTileCount; ++t) {
        for (std::size_t p = 0; p < tilePixels_[t].size(); ++p) {
            const std::uint8_t byte = patterns[t][p / kPixelsPerByte];
            // TBL-TilePattern: 4 bits/pixel, 2 pixels/byte, low nibble = left pixel.
            const auto index = static_cast<std::uint8_t>(
                (p % kPixelsPerByte == 0 ? byte : byte >> kNibbleBits) & kNibbleMask);
            tilePixels_[t][p] = kOpaque | (palette[index] & kRgbMask);
        }
    }
    return ApiStatus::Ok;
}

ApiStatus SdlBackend::setInputContext(InputContext context) noexcept {
    if (context == InputContext::Global) {
        return ApiStatus::InvalidArgument;  // SR-INP-09: Global is never current
    }
    context_ = context;
    return ApiStatus::Ok;
}

std::uint32_t SdlBackend::pixelAt(int x, int y) const noexcept {
    if (x < 0 || x >= kWindowWidth || y < 0 || y >= kWindowHeight) {
        return 0;
    }
    return (*pixels_)[static_cast<std::size_t>(y) * kWindowWidth + static_cast<std::size_t>(x)] &
           kRgbMask;
}

}  // namespace arcade
