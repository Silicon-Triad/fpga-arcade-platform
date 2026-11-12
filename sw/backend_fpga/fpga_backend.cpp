// FPGA backend of the Arcade API (SR-FW-05, QA-03).
#include "backend_fpga/fpga_backend.hpp"

#include <type_traits>

#include "assets/text_tiles.hpp"

namespace arcade {
namespace {

// The frozen TileSet and Palette (TBL-TilePattern, TBL-Palette) have the same layout as the
// HAL's own types, so loadTiles() passes them on without a copy or repacking.
static_assert(std::is_same_v<TileSet, hal::HalTileSet>, "TBL-TilePattern: same layout");
static_assert(std::is_same_v<Palette, hal::HalPalette>, "TBL-Palette: same layout");

ApiStatus drawStatus(hal::HalStatus status) noexcept {
    return status == hal::HalStatus::kOk ? ApiStatus::Ok : ApiStatus::OutOfRange;
}

}  // namespace

FpgaBackend::FpgaBackend(hal::RegisterSpace& regs, hal::MonotonicClock& clock, InputManager& input,
                         const KeyBindings& bindings, FpgaSound* sound) noexcept
    : tilemap_(regs, clock), lifecycle_(regs), input_(input), bindings_(bindings), sound_(sound) {}

FpgaBackend::~FpgaBackend() { shutdown(); }

ApiStatus FpgaBackend::setTile(int col, int row, TileIndex tile) noexcept {
    return drawStatus(tilemap_.setTile(col, row, tile));  // SR-HAL-03
}

ApiStatus FpgaBackend::fillRect(int col, int row, int width, int height, TileIndex tile) noexcept {
    // The HAL accepts an empty rectangle; SdlBackend does not, and both backends must agree.
    if (width <= 0 || height <= 0) {
        return ApiStatus::OutOfRange;
    }
    return drawStatus(tilemap_.fillRect(col, row, width, height, tile));  // SR-HAL-04
}

ApiStatus FpgaBackend::drawText(int col, int row, std::string_view text, TextScale scale) noexcept {
    // SR-FW-12: the same layout the SDL2 backend uses (FontTextLayout), so text gives the same
    // tiles on both. It checks everything before the first cell, so on an error nothing is
    // drawn; every cell it sends is inside the grid.
    return assets::textToTiles(
        col, row, text, scale,
        [](void* context, int cellCol, int cellRow, TileIndex tile) noexcept {
            (void)static_cast<hal::TilemapHal*>(context)->setTile(cellCol, cellRow, tile);
        },
        &tilemap_);
}

void FpgaBackend::clear(TileIndex tile) noexcept {
    (void)tilemap_.fillRect(0, 0, kGridCols, kGridRows, tile);  // the whole grid: always kOk
}

ApiStatus FpgaBackend::present() noexcept {
    if (lifecycle_.isShutDown()) {
        return ApiStatus::BackendError;  // SR-HAL-07: no bridge access after shutdown
    }
    if (sound_ != nullptr) {
        sound_->tick();  // SR-HAL-11: next note, once per tick (#44)
    }
    // SR-HAL-08, SR-HAL-05: a swap not confirmed within VSYNC_TIMEOUT is a backend error.
    return tilemap_.present() == hal::HalStatus::kOk ? ApiStatus::Ok : ApiStatus::BackendError;
}

void FpgaBackend::push(const InputEvent& event) noexcept {
    const bool keyboardEvent =
        event.type == InputEventType::KeyboardLost || event.type == InputEventType::KeyboardFound;
    const std::size_t limit = keyboardEvent ? kQueueCapacity : kQueueCapacity - kReservedSlots;
    if (queueSize_ >= limit) {
        ++dropped_;  // see kQueueCapacity
        return;
    }
    queue_[(queueHead_ + queueSize_) % kQueueCapacity] = event;
    ++queueSize_;
}

void FpgaBackend::onKeyEvent(const KeyEvent& event) noexcept {
    switch (event.type) {
        case KeyEventType::KeyDown: {
            // SR-INP-09 to SR-INP-11: current context, then Global; unbound keys are dropped
            // (SR-INP-10).
            const std::optional<InputEvent> mapped =
                bindings_.translateKeyDown(context_, event.code);
            if (event.code < kKeyCount) {
                pressEvent_[event.code] = mapped;
            }
            if (mapped) {
                push(*mapped);
            }
            break;
        }
        case KeyEventType::KeyUp: {
            // The KeyUp reports its KeyDown's action, even if the context changed meanwhile.
            if (event.code >= kKeyCount) {
                break;
            }
            std::optional<InputEvent>& down = pressEvent_[event.code];
            if (down.has_value()) {
                InputEvent up = *down;
                up.type = InputEventType::KeyUp;
                down.reset();
                push(up);
            }
            break;
        }
        case KeyEventType::KeyboardLost:
            // SR-INP-13: every key is released, without KeyUp events (sw/input/README.md).
            pressEvent_.fill(std::nullopt);
            push(InputEvent{InputEventType::KeyboardLost, Action::Up, '\0'});  // SR-INP-14
            break;
        case KeyEventType::KeyboardFound:
            push(InputEvent{InputEventType::KeyboardFound, Action::Up, '\0'});  // SR-INP-15
            break;
    }
}

InputFrame FpgaBackend::pollInput() noexcept {
    // SR-INP-02, SR-INP-04: the Input Manager reads without blocking until EAGAIN. Read it only
    // once the backlog is delivered, so a long burst waits in the kernel queue (kQueueCapacity).
    if (queueSize_ == 0) {
        input_.poll([this](const KeyEvent& event) { onKeyEvent(event); });
    }

    InputFrame frame;
    while (frame.count < kMaxEventsPerTick && queueSize_ > 0) {
        frame.events[frame.count++] = queue_[queueHead_];
        queueHead_ = (queueHead_ + 1) % kQueueCapacity;
        --queueSize_;
    }
    // App. A Held-key state: held keys through the current context, then Global (SR-INP-08,
    // SR-INP-09), as in SdlBackend.
    for (std::size_t code = 0; code < kKeyCount; ++code) {
        const auto key = static_cast<std::uint16_t>(code);
        if (input_.isPressed(key)) {
            if (const std::optional<Action> action = bindings_.lookup(context_, key)) {
                frame.held[static_cast<std::size_t>(*action)] = true;
            }
        }
    }
    return frame;
}

void FpgaBackend::playSound(EffectId effectId) noexcept {
    // SR-FW-07: every call is accepted. Silent until #44 supplies the driver.
    if (sound_ != nullptr) {
        sound_->play(effectId);
    }
}

void FpgaBackend::setMute(bool on) noexcept {
    muted_ = on;  // SR-FW-07, SR-FW-11
    if (sound_ != nullptr) {
        sound_->setMute(on);  // SR-HAL-12 (#44)
    }
}

ApiStatus FpgaBackend::loadTiles(const TileSet& patterns, const Palette& palette) noexcept {
    // SR-HAL-13: Display Enable is written only after patterns and palette, and not at all on
    // an error.
    switch (lifecycle_.loadAssets(patterns, palette)) {
        case hal::HalStatus::kOk:
            return ApiStatus::Ok;
        case hal::HalStatus::kInvalidPalette:
            return ApiStatus::InvalidArgument;  // an entry is not 0x00RRGGBB (TBL-Palette)
        default:
            return ApiStatus::BackendError;  // kShutDown: SR-HAL-07
    }
}

std::uint32_t FpgaBackend::frameCount() const noexcept {
    if (!lifecycle_.isShutDown()) {
        lastFrameCount_ = tilemap_.frameCounter();  // SR-HAL-06: non-blocking register read
    }
    return lastFrameCount_;
}

ApiStatus FpgaBackend::setInputContext(InputContext context) noexcept {
    if (context == InputContext::Global) {
        return ApiStatus::InvalidArgument;  // SR-INP-09: Global is never current
    }
    context_ = context;
    return ApiStatus::Ok;
}

void FpgaBackend::shutdown() noexcept {
    lifecycle_.shutdown();  // SR-HAL-07: releases once, writes no register
}

}  // namespace arcade
