// Unit tests for the SDL2 backend, headless through SDL's dummy video driver (issue #17,
// SR-FW-05 to SR-FW-07).
#include "backend_sdl/sdl_backend.hpp"

#include <SDL2/SDL.h>
#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "backend_sdl/font_text_layout.hpp"

namespace arcade {
namespace {

/// Fake TBL-KeyBinding lookup (the real one is issue #21): fixed bindings for one context,
/// and a record of every context it was asked about.
class FakeLookup : public KeyActionLookup {
  public:
    std::optional<KeyMapping> lookup(InputContext context,
                                     std::uint16_t keyCode) const noexcept override {
        contexts.push_back(context);
        const auto it = bindings.find({context, keyCode});
        if (it == bindings.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::map<std::pair<InputContext, std::uint16_t>, KeyMapping> bindings;
    mutable std::vector<InputContext> contexts;
};

class SdlBackendTest : public ::testing::Test {
  protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");  // CI has no display
        backend = SdlBackend::open(lookup);
        ASSERT_NE(backend, nullptr) << SDL_GetError();
    }

    static void pushKey(Uint32 type, SDL_Scancode scancode, Uint8 repeat = 0) {
        SDL_Event e{};
        e.type = type;
        e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
        e.key.repeat = repeat;
        e.key.keysym.scancode = scancode;
        ASSERT_EQ(SDL_PushEvent(&e), 1) << SDL_GetError();
    }

    FakeLookup lookup;
    std::unique_ptr<SdlBackend> backend;
};

TEST_F(SdlBackendTest, SR_FW_07_EverySoundCallSucceeds) {
    for (std::size_t i = 0; i < kEffectCount; ++i) {
        backend->playSound(static_cast<EffectId>(i));
    }
    backend->setMute(true);
    EXPECT_TRUE(backend->muted());
    for (std::size_t i = 0; i < kEffectCount; ++i) {
        backend->playSound(static_cast<EffectId>(i));  // accepted while muted too
    }
    backend->setMute(false);
    EXPECT_FALSE(backend->muted());
    // The backend is still usable after every sound call.
    EXPECT_EQ(backend->present(), ApiStatus::Ok);
}

// ---- Tilemap: bounds and return codes (App. A Tilemap drawing) ----------------------------

TileSet distinctTiles() {
    // Tile t is filled with palette index (t % 16), so each cell's tile shows in its pixels.
    TileSet tiles{};
    for (std::size_t t = 0; t < kTileCount; ++t) {
        const auto nibble = static_cast<std::uint8_t>(t % kPaletteSize);
        tiles[t].fill(static_cast<std::uint8_t>(nibble | (nibble << 4)));
    }
    return tiles;
}

Palette distinctPalette() {
    Palette palette{};
    for (std::size_t i = 0; i < kPaletteSize; ++i) {
        palette[i] = 0x00010101U * static_cast<std::uint32_t>(i + 1);
    }
    return palette;
}

class SdlTilemapTest : public SdlBackendTest {
  protected:
    void SetUp() override {
        SdlBackendTest::SetUp();
        ASSERT_EQ(backend->loadTiles(distinctTiles(), distinctPalette()), ApiStatus::Ok);
    }
    /// Colour shown at cell (col, row) after present().
    std::uint32_t cell(int col, int row) const {
        return backend->pixelAt(col * kTilePx, row * kTilePx);
    }
    static std::uint32_t colourOf(TileIndex tile) { return distinctPalette()[tile % kPaletteSize]; }
};

TEST_F(SdlTilemapTest, SR_FW_06_SetTileInsideGrid) {
    EXPECT_EQ(backend->setTile(0, 0, 1), ApiStatus::Ok);
    EXPECT_EQ(backend->setTile(kGridCols - 1, kGridRows - 1, 2), ApiStatus::Ok);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    EXPECT_EQ(cell(0, 0), colourOf(1));
    EXPECT_EQ(cell(kGridCols - 1, kGridRows - 1), colourOf(2));
    EXPECT_EQ(cell(1, 0), colourOf(0));
}

TEST_F(SdlTilemapTest, SR_FW_06_SetTileOutsideGridIsOutOfRange) {
    EXPECT_EQ(backend->setTile(-1, 0, 3), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->setTile(0, -1, 3), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->setTile(kGridCols, 0, 3), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->setTile(0, kGridRows, 3), ApiStatus::OutOfRange);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            ASSERT_EQ(cell(col, row), colourOf(0)) << col << "," << row;
        }
    }
}

TEST_F(SdlTilemapTest, SR_FW_06_FillRectInsideGrid) {
    EXPECT_EQ(backend->fillRect(2, 3, 4, 5, 7), ApiStatus::Ok);
    EXPECT_EQ(backend->fillRect(kGridCols - 1, kGridRows - 1, 1, 1, 8), ApiStatus::Ok);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    EXPECT_EQ(cell(2, 3), colourOf(7));
    EXPECT_EQ(cell(5, 7), colourOf(7));
    EXPECT_EQ(cell(6, 7), colourOf(0));
    EXPECT_EQ(cell(5, 8), colourOf(0));
    EXPECT_EQ(cell(1, 3), colourOf(0));
    EXPECT_EQ(cell(kGridCols - 1, kGridRows - 1), colourOf(8));
}

TEST_F(SdlTilemapTest, SR_FW_06_FillRectPartlyOutsideDrawsNothing) {
    EXPECT_EQ(backend->fillRect(kGridCols - 2, 0, 3, 1, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(0, kGridRows - 2, 1, 3, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(-1, 0, 2, 2, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(0, -1, 2, 2, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(0, 0, 0, 1, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(0, 0, 1, -1, 9), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->fillRect(1, 1, 0x7FFFFFFF, 1, 9), ApiStatus::OutOfRange);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    EXPECT_EQ(cell(kGridCols - 2, 0), colourOf(0));
    EXPECT_EQ(cell(0, kGridRows - 2), colourOf(0));
    EXPECT_EQ(cell(0, 0), colourOf(0));
    EXPECT_EQ(cell(1, 1), colourOf(0));
}

TEST_F(SdlTilemapTest, SR_FW_06_ClearSetsEveryCell) {
    ASSERT_EQ(backend->setTile(10, 10, 4), ApiStatus::Ok);
    backend->clear(6);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            ASSERT_EQ(cell(col, row), colourOf(6)) << col << "," << row;
        }
    }
}

TEST_F(SdlTilemapTest, SR_FW_12_DrawTextWithoutLayoutHookIsBackendError) {
    EXPECT_EQ(backend->drawText(0, 0, "A", TextScale::X1), ApiStatus::BackendError);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);
    EXPECT_EQ(cell(0, 0), colourOf(0));
}

// ---- Pixel output (TBL-TilePattern, TBL-Palette) -------------------------------------------

TEST_F(SdlBackendTest, SR_FW_06_PixelColourLowNibbleIsLeftPixel) {
    constexpr TileIndex kTile = 5;
    TileSet tiles{};
    tiles[kTile][0] = 0x21;  // row 0: pixel 0 = index 1 (low nibble), pixel 1 = index 2
    tiles[kTile][3] = 0xF0;  // row 0: pixel 6 = index 0, pixel 7 = index 15
    tiles[kTile][4] = 0x03;  // row 1: pixel 0 = index 3
    Palette palette{};
    palette[0] = 0x00000000U;
    palette[1] = 0x00112233U;
    palette[2] = 0x00AABBCCU;
    palette[3] = 0x00FF0000U;
    palette[15] = 0x0000FF00U;
    ASSERT_EQ(backend->loadTiles(tiles, palette), ApiStatus::Ok);
    ASSERT_EQ(backend->setTile(3, 4, kTile), ApiStatus::Ok);
    ASSERT_EQ(backend->present(), ApiStatus::Ok);

    const int x0 = 3 * kTilePx;
    const int y0 = 4 * kTilePx;
    EXPECT_EQ(backend->pixelAt(x0, y0), 0x00112233U);
    EXPECT_EQ(backend->pixelAt(x0 + 1, y0), 0x00AABBCCU);
    EXPECT_EQ(backend->pixelAt(x0 + 6, y0), 0x00000000U);
    EXPECT_EQ(backend->pixelAt(x0 + 7, y0), 0x0000FF00U);
    EXPECT_EQ(backend->pixelAt(x0, y0 + 1), 0x00FF0000U);
    EXPECT_EQ(backend->pixelAt(x0 - 1, y0), 0x00000000U);  // neighbour cell, tile 0
}

// ---- Input (SR-FW-06, SR-INP-07 to SR-INP-09) ----------------------------------------------

TEST_F(SdlBackendTest, SR_FW_06_SdlKeysBecomeLinuxKeyCodes) {
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_A), std::optional<std::uint16_t>(linux_key::kA));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_Z), std::optional<std::uint16_t>(linux_key::kZ));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_M), std::optional<std::uint16_t>(linux_key::kM));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_1), std::optional<std::uint16_t>(linux_key::k1));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_9), std::optional<std::uint16_t>(linux_key::k1 + 8));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_0), std::optional<std::uint16_t>(linux_key::k0));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_RETURN), std::optional<std::uint16_t>(linux_key::kEnter));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_ESCAPE), std::optional<std::uint16_t>(linux_key::kEsc));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_UP), std::optional<std::uint16_t>(linux_key::kUp));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_DOWN), std::optional<std::uint16_t>(linux_key::kDown));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_LEFT), std::optional<std::uint16_t>(linux_key::kLeft));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_RIGHT), std::optional<std::uint16_t>(linux_key::kRight));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_RCTRL),
              std::optional<std::uint16_t>(linux_key::kRightCtrl));
    EXPECT_EQ(linuxKeyCode(SDL_SCANCODE_F12), std::nullopt);

    // Through the backend: the lookup is asked with the Linux code.
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_RETURN);
    (void)backend->pollInput();
    EXPECT_TRUE(backend->isKeyHeld(linux_key::kEnter));
}

TEST_F(SdlBackendTest, SR_FW_06_KeyDownKeyUpAndHeldState) {
    lookup.bindings[{InputContext::Menu, linux_key::kUp}] = {Action::Up, 0};
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_UP);
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_UP, 1);  // auto-repeat: discarded (SR-INP-07)

    InputFrame f = backend->pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].type, InputEventType::KeyDown);
    EXPECT_EQ(f.events[0].action, Action::Up);
    EXPECT_TRUE(f.held[static_cast<std::size_t>(Action::Up)]);

    f = backend->pollInput();  // still held, no new events
    EXPECT_EQ(f.count, 0U);
    EXPECT_TRUE(f.held[static_cast<std::size_t>(Action::Up)]);

    pushKey(SDL_KEYUP, SDL_SCANCODE_UP);
    f = backend->pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].type, InputEventType::KeyUp);
    EXPECT_EQ(f.events[0].action, Action::Up);
    EXPECT_FALSE(f.held[static_cast<std::size_t>(Action::Up)]);
    EXPECT_FALSE(backend->isKeyHeld(linux_key::kUp));
}

TEST_F(SdlBackendTest, SR_INP_09_ActionsComeFromLookupWithCurrentContext) {
    lookup.bindings[{InputContext::Snake, linux_key::kW}] = {Action::Up, 0};
    lookup.bindings[{InputContext::Initials, linux_key::kW}] = {Action::Char, 'W'};

    EXPECT_EQ(backend->setInputContext(InputContext::Global), ApiStatus::InvalidArgument);
    ASSERT_EQ(backend->setInputContext(InputContext::Snake), ApiStatus::Ok);
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_W);
    pushKey(SDL_KEYUP, SDL_SCANCODE_W);
    lookup.contexts.clear();
    InputFrame f = backend->pollInput();
    ASSERT_EQ(f.count, 2U);
    EXPECT_EQ(f.events[0].action, Action::Up);
    EXPECT_EQ(f.events[0].ch, 0);
    for (InputContext c : lookup.contexts) {
        EXPECT_EQ(c, InputContext::Snake);
    }

    ASSERT_EQ(backend->setInputContext(InputContext::Initials), ApiStatus::Ok);
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_W);
    f = backend->pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].action, Action::Char);
    EXPECT_EQ(f.events[0].ch, 'W');

    // An unbound key (S in Initials) produces no event.
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_S);
    f = backend->pollInput();
    EXPECT_EQ(f.count, 0U);
}

TEST_F(SdlBackendTest, SR_FW_06_KeyUpUsesActionOfItsKeyDown) {
    // Enter selects on the menu; the screen changes to Snake before Enter is released.
    lookup.bindings[{InputContext::Menu, linux_key::kEnter}] = {Action::Select, 0};
    lookup.bindings[{InputContext::Snake, linux_key::kEnter}] = {Action::Pause, 0};
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_RETURN);
    InputFrame f = backend->pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].action, Action::Select);

    ASSERT_EQ(backend->setInputContext(InputContext::Snake), ApiStatus::Ok);
    pushKey(SDL_KEYUP, SDL_SCANCODE_RETURN);
    f = backend->pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].type, InputEventType::KeyUp);
    EXPECT_EQ(f.events[0].action, Action::Select);

    // A key pressed while unbound gives no KeyUp, even if bound at release.
    ASSERT_EQ(backend->setInputContext(InputContext::Menu), ApiStatus::Ok);
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_P);
    EXPECT_EQ(backend->pollInput().count, 0U);
    lookup.bindings[{InputContext::Menu, linux_key::kP}] = {Action::Pause, 0};
    pushKey(SDL_KEYUP, SDL_SCANCODE_P);
    EXPECT_EQ(backend->pollInput().count, 0U);
}

TEST_F(SdlBackendTest, SR_INP_02_EventsBeyondMaxPerTickCarryOver) {
    lookup.bindings[{InputContext::Menu, linux_key::kLeft}] = {Action::Left, 0};
    lookup.bindings[{InputContext::Menu, linux_key::kRight}] = {Action::Right, 0};
    constexpr std::size_t kPairs = kMaxEventsPerTick;  // 2 x kMaxEventsPerTick events
    for (std::size_t i = 0; i < kPairs; ++i) {
        pushKey(SDL_KEYDOWN, i % 2 == 0 ? SDL_SCANCODE_LEFT : SDL_SCANCODE_RIGHT);
        pushKey(SDL_KEYUP, i % 2 == 0 ? SDL_SCANCODE_LEFT : SDL_SCANCODE_RIGHT);
    }
    pushKey(SDL_KEYDOWN, SDL_SCANCODE_LEFT);  // one more: 2 x max + 1 in total

    std::vector<InputEvent> all;
    for (int tick = 0; tick < 3; ++tick) {
        const InputFrame f = backend->pollInput();
        EXPECT_EQ(f.count, tick < 2 ? kMaxEventsPerTick : 1U) << "tick " << tick;
        all.insert(all.end(), f.events.begin(),
                   f.events.begin() + static_cast<std::ptrdiff_t>(f.count));
    }
    ASSERT_EQ(all.size(), 2 * kPairs + 1);
    for (std::size_t i = 0; i < 2 * kPairs; ++i) {  // in order, none dropped
        const std::size_t pair = i / 2;
        EXPECT_EQ(all[i].type, i % 2 == 0 ? InputEventType::KeyDown : InputEventType::KeyUp);
        EXPECT_EQ(all[i].action, pair % 2 == 0 ? Action::Left : Action::Right);
    }
    EXPECT_EQ(all.back().action, Action::Left);
    EXPECT_EQ(backend->pollInput().count, 0U);
}

TEST_F(SdlBackendTest, SR_INP_02_MoreEventsThanInternalQueueAreKeptInOrder) {
    lookup.bindings[{InputContext::Menu, linux_key::kA}] = {Action::Select, 0};
    constexpr std::size_t kEvents = 20 * kMaxEventsPerTick;  // beyond the internal queue
    for (std::size_t i = 0; i < kEvents / 2; ++i) {
        pushKey(SDL_KEYDOWN, SDL_SCANCODE_A);
        pushKey(SDL_KEYUP, SDL_SCANCODE_A);
    }
    std::size_t total = 0;
    for (std::size_t tick = 0; tick < 25; ++tick) {
        const InputFrame f = backend->pollInput();
        for (std::size_t i = 0; i < f.count; ++i, ++total) {
            ASSERT_EQ(f.events[i].type,
                      total % 2 == 0 ? InputEventType::KeyDown : InputEventType::KeyUp);
        }
    }
    EXPECT_EQ(total, kEvents);
}

// ---- Frame counter (SR-FW-02) --------------------------------------------------------------

TEST_F(SdlBackendTest, SR_FW_02_FrameCountAdvancesOncePerPresent) {
    ASSERT_EQ(backend->present(), ApiStatus::Ok);  // align to a frame boundary
    for (int i = 0; i < 5; ++i) {
        const std::uint32_t before = backend->frameCount();
        ASSERT_EQ(backend->present(), ApiStatus::Ok);
        EXPECT_EQ(backend->frameCount(), before + 1);
    }
}

TEST(SdlFontText, SR_FW_12_DrawTextWorksThroughFontLayout) {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");  // CI has no display
    FakeLookup lookup;
    const FontTextLayout layout;
    auto backend = SdlBackend::open(lookup, &layout);
    ASSERT_NE(backend, nullptr) << SDL_GetError();
    EXPECT_EQ(backend->drawText(2, 1, "SNAKE", TextScale::X1), ApiStatus::Ok);
    EXPECT_EQ(backend->drawText(13, 28, "SNAKE", TextScale::X4), ApiStatus::Ok);
    EXPECT_EQ(backend->drawText(127, 0, "AB", TextScale::X1), ApiStatus::OutOfRange);
    EXPECT_EQ(backend->drawText(0, 0, "\n", TextScale::X1), ApiStatus::InvalidArgument);
}

}  // namespace
}  // namespace arcade
