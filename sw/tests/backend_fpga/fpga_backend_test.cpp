// Unit tests for the FPGA backend on the mock register space, a fake clock and a fake evdev
// (issue #37, SR-FW-05, QA-03). Register offsets are never named here: bridge traffic is
// compared with what the HAL itself does for the same calls, whose addresses
// sw/tests/hal/ checks.
#include "backend_fpga/fpga_backend.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "../hal/mock_register_space.hpp"
#include "assets/palette.hpp"
#include "backend_sdl/font_text_layout.hpp"
#include "fake_evdev_sys.hpp"

#ifdef ARCADE_HAS_SDL
#include <SDL2/SDL.h>

#include "app/binding_lookup.hpp"
#include "backend_sdl/sdl_backend.hpp"
#endif

namespace arcade {
namespace {

using hal::test::Access;
using hal::test::FakeClock;
using hal::test::MockRegisterSpace;
using test::FakeEvdevSys;

/// The bridge window, clock, keyboard and bindings a backend binds to.
struct Board {
    Board() { EXPECT_TRUE(input.start()); }
    MockRegisterSpace regs;
    FakeClock clock;
    FakeEvdevSys evdev;
    InputManager input{evdev};
    KeyBindings bindings;
};

/// Records every call forwarded to the sound driver hook (#44).
class FakeSound final : public FpgaSound {
  public:
    void play(EffectId effectId) noexcept override { played.push_back(effectId); }
    void setMute(bool on) noexcept override { mutes.push_back(on); }
    void tick() noexcept override { ++ticks; }
    std::vector<EffectId> played;
    std::vector<bool> mutes;
    int ticks = 0;
};

class FpgaBackendTest : public ::testing::Test {
  protected:
    Board board;
    FpgaBackend backend{board.regs, board.clock, board.input, board.bindings};
};

TileSet distinctTiles() {
    TileSet tiles{};
    for (std::size_t t = 0; t < kTileCount; ++t) {
        for (std::size_t b = 0; b < kTilePatternBytes; ++b) {
            tiles[t][b] = static_cast<std::uint8_t>(t * 7 + b * 13 + 1);
        }
    }
    return tiles;
}

bool isWrite(const Access& a) {
    return a.kind == Access::Kind::kWrite8 || a.kind == Access::Kind::kWrite32;
}

// ---- Return codes: the same as SdlBackend for the same calls (SR-FW-05) --------------------

/// Every drawing and context call with an expected status. The statuses are SdlBackend's
/// (sw/backend_sdl/sdl_backend.cpp); with SDL2 in the build the same table runs on it too.
template <typename Api>
void expectSameReturnCodes(Api& api) {
    struct Case {
        const char* what;
        ApiStatus status;
    };
    const std::vector<Case> cases = {
        {"setTile in grid", api.setTile(0, 0, 1)},
        {"setTile last cell", api.setTile(kGridCols - 1, kGridRows - 1, 2)},
        {"setTile col -1", api.setTile(-1, 0, 1)},
        {"setTile row -1", api.setTile(0, -1, 1)},
        {"setTile col 128", api.setTile(kGridCols, 0, 1)},
        {"setTile row 96", api.setTile(0, kGridRows, 1)},
        {"fillRect whole grid", api.fillRect(0, 0, kGridCols, kGridRows, 3)},
        {"fillRect 1x1", api.fillRect(5, 5, 1, 1, 3)},
        {"fillRect width 0", api.fillRect(5, 5, 0, 1, 3)},
        {"fillRect height 0", api.fillRect(5, 5, 1, 0, 3)},
        {"fillRect negative width", api.fillRect(5, 5, -1, 1, 3)},
        {"fillRect too wide", api.fillRect(1, 0, kGridCols, 1, 3)},
        {"fillRect too tall", api.fillRect(0, 1, 1, kGridRows, 3)},
        {"fillRect col -1", api.fillRect(-1, 0, 2, 2, 3)},
        {"fillRect huge", api.fillRect(1, 1, 2147483647, 2147483647, 3)},
        {"drawText X1", api.drawText(0, 0, "SNAKE 0123", TextScale::X1)},
        {"drawText X4", api.drawText(0, 10, "AB", TextScale::X4)},
        {"drawText empty", api.drawText(0, 0, "", TextScale::X1)},
        {"drawText too long", api.drawText(kGridCols - 2, 0, "ABC", TextScale::X1)},
        {"drawText X4 too low", api.drawText(0, kGridRows - 3, "A", TextScale::X4)},
        {"drawText row -1", api.drawText(0, -1, "A", TextScale::X1)},
        {"drawText no glyph", api.drawText(0, 0, "a\x01", TextScale::X1)},
        {"drawText bad scale", api.drawText(0, 0, "A", static_cast<TextScale>(2))},
        {"setInputContext Global", api.setInputContext(InputContext::Global)},
        {"setInputContext Snake", api.setInputContext(InputContext::Snake)},
        {"setInputContext ShutdownConfirm", api.setInputContext(InputContext::ShutdownConfirm)},
    };
    const std::vector<ApiStatus> expected = {
        ApiStatus::Ok,
        ApiStatus::Ok,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::Ok,
        ApiStatus::Ok,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::Ok,
        ApiStatus::Ok,
        ApiStatus::Ok,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::OutOfRange,
        ApiStatus::InvalidArgument,
        ApiStatus::InvalidArgument,
        ApiStatus::InvalidArgument,
        ApiStatus::Ok,
        ApiStatus::Ok,
    };
    ASSERT_EQ(cases.size(), expected.size());
    for (std::size_t i = 0; i < cases.size(); ++i) {
        EXPECT_EQ(cases[i].status, expected[i]) << cases[i].what;
    }
}

TEST_F(FpgaBackendTest, SR_FW_05_ReturnCodesMatchSdlBackend) { expectSameReturnCodes(backend); }

TEST_F(FpgaBackendTest, SR_FW_05_OutOfRangeCallsDrawNothing) {
    backend.clear(9);
    EXPECT_EQ(backend.setTile(kGridCols, 0, 1), ApiStatus::OutOfRange);
    EXPECT_EQ(backend.fillRect(kGridCols - 1, 0, 2, 1, 1), ApiStatus::OutOfRange);
    EXPECT_EQ(backend.fillRect(0, 0, 0, 1, 1), ApiStatus::OutOfRange);
    EXPECT_EQ(backend.drawText(kGridCols - 1, 0, "AB", TextScale::X1), ApiStatus::OutOfRange);
    EXPECT_EQ(backend.drawText(0, 0, "A\x01", TextScale::X1), ApiStatus::InvalidArgument);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            ASSERT_EQ(backend.shadowTile(col, row), 9) << col << "," << row;
        }
    }
}

#ifdef ARCADE_HAS_SDL
class SdlParityTest : public ::testing::Test {
  protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");  // CI has no display
        sdl = SdlBackend::open(lookup, &layout);
        ASSERT_NE(sdl, nullptr) << SDL_GetError();
    }

    static void pushKey(bool down, SDL_Scancode scancode) {
        SDL_Event e{};
        e.type = down ? SDL_KEYDOWN : SDL_KEYUP;
        e.key.state = down ? SDL_PRESSED : SDL_RELEASED;
        e.key.keysym.scancode = scancode;
        ASSERT_EQ(SDL_PushEvent(&e), 1) << SDL_GetError();
    }

    Board board;
    FpgaBackend fpga{board.regs, board.clock, board.input, board.bindings};
    BindingLookup lookup{board.bindings};
    FontTextLayout layout;
    std::unique_ptr<SdlBackend> sdl;
};

TEST_F(SdlParityTest, SR_FW_05_SdlBackendGivesTheSameReturnCodes) {
    expectSameReturnCodes(*sdl);  // proves the expected table is SdlBackend's own
}

TEST_F(SdlParityTest, SR_FW_05_KeysMapToTheSameActionsAsSdlBackend) {
    struct Step {
        InputContext context;
        std::uint16_t code;
        SDL_Scancode scancode;
        bool down;
    };
    const std::vector<Step> steps = {
        {InputContext::Menu, KEY_UP, SDL_SCANCODE_UP, true},
        {InputContext::Menu, KEY_UP, SDL_SCANCODE_UP, false},
        {InputContext::Menu, KEY_ENTER, SDL_SCANCODE_RETURN, true},
        {InputContext::Snake, KEY_ENTER, SDL_SCANCODE_RETURN, false},  // KeyUp keeps Select
        {InputContext::Snake, KEY_LEFT, SDL_SCANCODE_LEFT, true},      // held across ticks
        {InputContext::Snake, KEY_Q, SDL_SCANCODE_Q, true},            // unbound: dropped
        {InputContext::Breakout, KEY_SPACE, SDL_SCANCODE_SPACE, true},
        {InputContext::Initials, KEY_M, SDL_SCANCODE_M, true},  // Char 'M', not Global Mute
        {InputContext::Menu, KEY_M, SDL_SCANCODE_M, false},
        {InputContext::Menu, KEY_M, SDL_SCANCODE_M, true},  // Global: Mute
        {InputContext::Breakout, KEY_LEFT, SDL_SCANCODE_LEFT, false},
    };
    for (const Step& s : steps) {
        ASSERT_EQ(fpga.setInputContext(s.context), ApiStatus::Ok);
        ASSERT_EQ(sdl->setInputContext(s.context), ApiStatus::Ok);
        if (s.down) {
            board.evdev.press(s.code);
        } else {
            board.evdev.release(s.code);
        }
        pushKey(s.down, s.scancode);
        const InputFrame a = fpga.pollInput();
        const InputFrame b = sdl->pollInput();
        ASSERT_EQ(a.count, b.count) << "key " << s.code;
        for (std::size_t i = 0; i < a.count; ++i) {
            EXPECT_EQ(a.events[i].type, b.events[i].type) << "key " << s.code;
            EXPECT_EQ(a.events[i].action, b.events[i].action) << "key " << s.code;
            EXPECT_EQ(a.events[i].ch, b.events[i].ch) << "key " << s.code;
        }
        EXPECT_EQ(a.held, b.held) << "key " << s.code;
    }
}
#endif  // ARCADE_HAS_SDL

// ---- Drawing: shadow only; present() is the bridge write (SR-HAL-03, 04, 08) ---------------

TEST_F(FpgaBackendTest, SR_HAL_03_SR_HAL_04_DrawingCallsMakeNoBridgeAccess) {
    EXPECT_EQ(backend.setTile(3, 4, 7), ApiStatus::Ok);
    EXPECT_EQ(backend.fillRect(10, 10, 20, 5, 8), ApiStatus::Ok);
    EXPECT_EQ(backend.drawText(0, 0, "HELLO", TextScale::X1), ApiStatus::Ok);
    EXPECT_EQ(backend.drawText(0, 20, "HI", TextScale::X4), ApiStatus::Ok);
    backend.clear(0);
    EXPECT_EQ(backend.setInputContext(InputContext::Snake), ApiStatus::Ok);
    (void)backend.pollInput();
    backend.playSound(EffectId::Eat);
    backend.setMute(true);
    EXPECT_TRUE(board.regs.log.empty());
}

/// The same calls on a bare TilemapHal over its own mock: the reference bridge traffic.
struct ReferenceHal {
    MockRegisterSpace regs;
    FakeClock clock;
    hal::TilemapHal hal{regs, clock};
};

TEST(FpgaBackendPresent, SR_HAL_08_PresentWritesTheBackBankThenRequestsTheSwap) {
    for (const std::uint32_t front : {0U, 1U}) {
        Board board;
        ReferenceHal ref;
        board.regs.front = front;
        ref.regs.front = front;
        FpgaBackend backend(board.regs, board.clock, board.input, board.bindings);
        ASSERT_EQ(backend.setTile(1, 2, 0x41), ApiStatus::Ok);
        ASSERT_EQ(backend.fillRect(50, 60, 4, 3, 0x42), ApiStatus::Ok);
        ASSERT_EQ(ref.hal.setTile(1, 2, 0x41), hal::HalStatus::kOk);
        ASSERT_EQ(ref.hal.fillRect(50, 60, 4, 3, 0x42), hal::HalStatus::kOk);
        ASSERT_TRUE(board.regs.log.empty());

        EXPECT_EQ(backend.present(), ApiStatus::Ok);
        ASSERT_EQ(ref.hal.present(), hal::HalStatus::kOk);
        // Exactly the traffic of TilemapHal::present(), whose back-bank addressing
        // tilemap_hal_test checks.
        ASSERT_EQ(board.regs.log.size(), ref.regs.log.size()) << "front " << front;
        for (std::size_t i = 0; i < ref.regs.log.size(); ++i) {
            ASSERT_EQ(board.regs.log[i].kind, ref.regs.log[i].kind) << i;
            ASSERT_EQ(board.regs.log[i].offset, ref.regs.log[i].offset) << i;
            ASSERT_EQ(board.regs.log[i].value, ref.regs.log[i].value) << i;
        }
        // Order (SR-HAL-08): cell writes, then exactly one 32-bit write (the swap request),
        // then the Status poll (reads only).
        std::size_t swapAt = board.regs.log.size();
        std::size_t cellWrites = 0;
        for (std::size_t i = 0; i < board.regs.log.size(); ++i) {
            const Access& a = board.regs.log[i];
            if (a.kind == Access::Kind::kWrite32) {
                EXPECT_EQ(swapAt, board.regs.log.size()) << "second 32-bit write";
                swapAt = i;
            } else if (a.kind == Access::Kind::kWrite8) {
                EXPECT_LT(i, swapAt) << "cell written after the swap request";
                ++cellWrites;
            }
        }
        ASSERT_LT(swapAt, board.regs.log.size()) << "no swap request";
        EXPECT_EQ(cellWrites, static_cast<std::size_t>(kGridCols) * kGridRows);  // first: all
        EXPECT_EQ(board.regs.log.back().kind, Access::Kind::kRead32);            // the Status poll
    }
}

TEST_F(FpgaBackendTest, SR_HAL_08_PresentCopiesTheShadowCellValues) {
    ReferenceHal ref;
    ASSERT_EQ(backend.drawText(4, 4, "GO", TextScale::X4), ApiStatus::Ok);
    ASSERT_EQ(assets::textToTiles(
                  4, 4, "GO", TextScale::X4,
                  [](void* hal, int c, int r, TileIndex t) noexcept {
                      (void)static_cast<hal::TilemapHal*>(hal)->setTile(c, r, t);
                  },
                  &ref.hal),
              ApiStatus::Ok);
    EXPECT_EQ(backend.present(), ApiStatus::Ok);
    ASSERT_EQ(ref.hal.present(), hal::HalStatus::kOk);
    EXPECT_EQ(board.regs.mem8, ref.regs.mem8);
}

TEST_F(FpgaBackendTest, SR_HAL_05_SwapTimeoutIsBackendError) {
    // Every Status bit stays set, so swap pending never clears; time runs on per poll.
    board.regs.status = 0xFFFFFFFFU;
    board.regs.onStatusRead = [this](int /*reads*/) {
        board.clock.t += std::chrono::milliseconds(10);
    };
    EXPECT_EQ(backend.present(), ApiStatus::BackendError);
    EXPECT_GT(board.clock.t, hal::VSYNC_TIMEOUT);
}

TEST_F(FpgaBackendTest, SR_HAL_06_FrameCountReadsTheFrameCounter) {
    board.regs.frameCounter = 1234;
    EXPECT_EQ(backend.frameCount(), 1234U);
    board.regs.frameCounter = 1235;
    EXPECT_EQ(backend.frameCount(), 1235U);
    EXPECT_EQ(board.regs.statusReads, 0);  // a plain register read: no vsync wait
}

// ---- drawText: the same tiles as SdlBackend (SR-FW-12) -------------------------------------

TEST_F(FpgaBackendTest, SR_FW_12_TextGivesTheSameTilesAsSdlBackend) {
    struct Text {
        int col;
        int row;
        std::string_view text;
        TextScale scale;
    };
    const std::vector<Text> texts = {
        {0, 0, "SAFE TO SWITCH OFF", TextScale::X1},
        {3, 7, "0123456789 ABCXYZ", TextScale::X1},
        {1, 20, "SNAKE", TextScale::X4},
        {kGridCols - 4, kGridRows - 4, "Z", TextScale::X4},
    };
    for (const Text& t : texts) {
        backend.clear(0);
        Tilemap sdlMap{};  // what SdlBackend::drawText writes into its shadow tilemap
        const FontTextLayout layout;
        ASSERT_EQ(layout.layout(t.col, t.row, t.text, t.scale, sdlMap), ApiStatus::Ok);
        ASSERT_EQ(backend.drawText(t.col, t.row, t.text, t.scale), ApiStatus::Ok);
        for (int row = 0; row < kGridRows; ++row) {
            for (int col = 0; col < kGridCols; ++col) {
                ASSERT_EQ(backend.shadowTile(col, row),
                          sdlMap[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)])
                    << t.text << " at " << col << "," << row;
            }
        }
    }
}

// ---- loadTiles (SR-HAL-13, SR-FW-01) -------------------------------------------------------

TEST_F(FpgaBackendTest, SR_HAL_13_LoadTilesWritesPatternsAndPaletteBeforeDisplayEnable) {
    const TileSet tiles = distinctTiles();
    ASSERT_EQ(backend.loadTiles(tiles, assets::kPalette), ApiStatus::Ok);

    // The same bridge traffic as HalLifecycle::loadAssets (whose addresses tests/hal checks).
    MockRegisterSpace ref;
    hal::HalLifecycle lifecycle(ref);
    ASSERT_EQ(lifecycle.loadAssets(tiles, assets::kPalette), hal::HalStatus::kOk);
    ASSERT_EQ(board.regs.log.size(), ref.log.size());
    for (std::size_t i = 0; i < ref.log.size(); ++i) {
        ASSERT_EQ(board.regs.log[i].kind, ref.log[i].kind) << i;
        ASSERT_EQ(board.regs.log[i].offset, ref.log[i].offset) << i;
        ASSERT_EQ(board.regs.log[i].value, ref.log[i].value) << i;
    }
    // Every pattern byte and palette entry is written before the last write, which is Control
    // with Display Enable set.
    std::size_t byteWrites = 0;
    std::size_t wordWrites = 0;
    for (std::size_t i = 0; i + 1 < board.regs.log.size(); ++i) {
        byteWrites += board.regs.log[i].kind == Access::Kind::kWrite8 ? 1U : 0U;
        wordWrites += board.regs.log[i].kind == Access::Kind::kWrite32 ? 1U : 0U;
    }
    EXPECT_EQ(byteWrites, kTileCount * kTilePatternBytes);
    EXPECT_EQ(wordWrites, kPaletteSize);
    EXPECT_EQ(board.regs.log.back().kind, Access::Kind::kWrite32);
    EXPECT_NE(board.regs.control, 0U);
    EXPECT_EQ(board.regs.control, board.regs.log.back().value);
}

TEST_F(FpgaBackendTest, SR_HAL_13_BadPaletteLeavesDisplayEnableClear) {
    Palette bad = assets::kPalette;
    bad[3] = 0x01000000U;  // bits above 0x00RRGGBB (TBL-Palette)
    EXPECT_EQ(backend.loadTiles(distinctTiles(), bad), ApiStatus::InvalidArgument);
    EXPECT_EQ(board.regs.control, 0U);
    EXPECT_TRUE(board.regs.log.empty());
}

// ---- Input (SR-INP-02, SR-INP-09 to SR-INP-16) ---------------------------------------------

bool hasEvent(const InputFrame& f, InputEventType type, Action action = Action::Up) {
    for (std::size_t i = 0; i < f.count; ++i) {
        const bool keyboard =
            type == InputEventType::KeyboardLost || type == InputEventType::KeyboardFound;
        if (f.events[i].type == type && (keyboard || f.events[i].action == action)) {
            return true;
        }
    }
    return false;
}

TEST_F(FpgaBackendTest, SR_INP_02_PollInputNeverBlocks) {
    for (int tick = 0; tick < 3; ++tick) {
        const int before = board.evdev.eagainReads;
        const InputFrame f = backend.pollInput();
        EXPECT_EQ(f.count, 0U);
        // The empty queue was seen as EAGAIN, so the read returned at once (SR-INP-04).
        EXPECT_EQ(board.evdev.eagainReads, before + 1);
    }
}

TEST_F(FpgaBackendTest, SR_INP_09_KeysMapThroughTheCurrentContext) {
    ASSERT_EQ(backend.setInputContext(InputContext::Snake), ApiStatus::Ok);
    board.evdev.press(KEY_UP);
    InputFrame f = backend.pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].type, InputEventType::KeyDown);
    EXPECT_EQ(f.events[0].action, Action::Up);
    EXPECT_TRUE(f.held[static_cast<std::size_t>(Action::Up)]);

    // Global fallback: M is Mute outside Initials.
    board.evdev.press(KEY_M);
    f = backend.pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].action, Action::Mute);

    // The KeyUp reports the KeyDown's action even after the context changed.
    ASSERT_EQ(backend.setInputContext(InputContext::Menu), ApiStatus::Ok);
    board.evdev.release(KEY_UP);
    f = backend.pollInput();
    ASSERT_EQ(f.count, 1U);
    EXPECT_EQ(f.events[0].type, InputEventType::KeyUp);
    EXPECT_EQ(f.events[0].action, Action::Up);
    EXPECT_FALSE(f.held[static_cast<std::size_t>(Action::Up)]);
}

TEST_F(FpgaBackendTest, SR_INP_10_UnboundKeysAreDropped) {
    ASSERT_EQ(backend.setInputContext(InputContext::Breakout), ApiStatus::Ok);
    board.evdev.press(KEY_UP);  // no Breakout or Global row
    board.evdev.release(KEY_UP);
    const InputFrame f = backend.pollInput();
    EXPECT_EQ(f.count, 0U);
    for (const bool held : f.held) {
        EXPECT_FALSE(held);
    }
}

TEST_F(FpgaBackendTest, SR_INP_11_InitialsLettersAreCharActions) {
    ASSERT_EQ(backend.setInputContext(InputContext::Initials), ApiStatus::Ok);
    board.evdev.press(KEY_Q);
    board.evdev.press(KEY_M);  // Char, not the Global Mute
    const InputFrame f = backend.pollInput();
    ASSERT_EQ(f.count, 2U);
    EXPECT_EQ(f.events[0].action, Action::Char);
    EXPECT_EQ(f.events[0].ch, 'Q');
    EXPECT_EQ(f.events[1].action, Action::Char);
    EXPECT_EQ(f.events[1].ch, 'M');
}

TEST_F(FpgaBackendTest, SR_INP_08_ManyEventsArriveInOrderOverSeveralTicks) {
    ASSERT_EQ(backend.setInputContext(InputContext::Menu), ApiStatus::Ok);
    constexpr std::size_t kPresses = kMaxEventsPerTick + 8;  // 80 events: three ticks
    for (std::size_t i = 0; i < kPresses; ++i) {
        board.evdev.press(i % 2 == 0 ? KEY_UP : KEY_DOWN);
        board.evdev.release(i % 2 == 0 ? KEY_UP : KEY_DOWN);
    }
    std::vector<InputEvent> all;
    for (int tick = 0; tick < 4; ++tick) {
        const InputFrame f = backend.pollInput();
        EXPECT_LE(f.count, kMaxEventsPerTick);
        all.insert(all.end(), f.events.begin(),
                   f.events.begin() + static_cast<std::ptrdiff_t>(f.count));
    }
    ASSERT_EQ(all.size(), 2 * kPresses);
    for (std::size_t i = 0; i < all.size(); ++i) {
        EXPECT_EQ(all[i].type, i % 2 == 0 ? InputEventType::KeyDown : InputEventType::KeyUp);
        EXPECT_EQ(all[i].action, (i / 2) % 2 == 0 ? Action::Up : Action::Down) << i;
    }
    EXPECT_EQ(backend.droppedEvents(), 0U);
}

TEST_F(FpgaBackendTest, SR_INP_14_KeyboardLostInTheSameTickAndKeysReleased) {
    ASSERT_EQ(backend.setInputContext(InputContext::Breakout), ApiStatus::Ok);
    board.evdev.press(KEY_LEFT);
    InputFrame f = backend.pollInput();
    ASSERT_TRUE(f.held[static_cast<std::size_t>(Action::Left)]);

    board.evdev.unplug();
    f = backend.pollInput();
    EXPECT_TRUE(hasEvent(f, InputEventType::KeyboardLost));          // SR-INP-14: same tick
    EXPECT_FALSE(f.held[static_cast<std::size_t>(Action::Left)]);    // SR-INP-13
    EXPECT_FALSE(hasEvent(f, InputEventType::KeyUp, Action::Left));  // state only, no event

    board.evdev.plugIn();
    f = backend.pollInput();
    EXPECT_TRUE(hasEvent(f, InputEventType::KeyboardFound));  // SR-INP-15

    board.evdev.press(KEY_SPACE);  // keys work again from the next tick
    f = backend.pollInput();
    EXPECT_TRUE(hasEvent(f, InputEventType::KeyDown, Action::Launch));
}

TEST_F(FpgaBackendTest, SR_INP_14_KeyboardLostIsNeverDroppedByAFullQueue) {
    ASSERT_EQ(backend.setInputContext(InputContext::Menu), ApiStatus::Ok);
    // One kernel drain with more key events than the backend queue holds, ending in ENODEV.
    constexpr std::size_t kKeyEvents = 8 * kMaxEventsPerTick + 10;
    for (std::size_t i = 0; i < kKeyEvents / 2; ++i) {
        board.evdev.press(KEY_UP);
        board.evdev.release(KEY_UP);
    }
    board.evdev.goneWhenEmpty = true;
    std::vector<InputEvent> all;
    for (int tick = 0; tick < 20; ++tick) {
        const InputFrame f = backend.pollInput();
        all.insert(all.end(), f.events.begin(),
                   f.events.begin() + static_cast<std::ptrdiff_t>(f.count));
    }
    ASSERT_FALSE(all.empty());
    EXPECT_EQ(all.back().type, InputEventType::KeyboardLost);  // last, after the keys
    // Key events beyond the queue are dropped and counted, never KeyboardLost
    // (kQueueCapacity, a design choice).
    EXPECT_GT(backend.droppedEvents(), 0U);
    EXPECT_EQ(all.size() + backend.droppedEvents(), kKeyEvents + 1);
}

// ---- Sound (SR-FW-07; driver hook for #44) -------------------------------------------------

TEST_F(FpgaBackendTest, SR_FW_07_EverySoundCallIsAcceptedWithoutADriver) {
    for (const bool mute : {false, true, false}) {
        backend.setMute(mute);
        EXPECT_EQ(backend.muted(), mute);
        for (std::size_t i = 0; i < kEffectCount; ++i) {
            backend.playSound(static_cast<EffectId>(i));
        }
    }
    EXPECT_TRUE(board.regs.log.empty());  // no audio registers before #44
    EXPECT_EQ(backend.present(), ApiStatus::Ok);
}

TEST(FpgaBackendSound, SR_FW_07_EverySoundCallReachesTheDriverHook) {
    Board board;
    FakeSound sound;
    FpgaBackend backend(board.regs, board.clock, board.input, board.bindings, &sound);
    backend.setMute(true);
    for (std::size_t i = 0; i < kEffectCount; ++i) {
        backend.playSound(static_cast<EffectId>(i));  // accepted while muted
    }
    backend.setMute(false);
    backend.playSound(EffectId::GameOver);
    EXPECT_EQ(sound.mutes, (std::vector<bool>{true, false}));
    ASSERT_EQ(sound.played.size(), kEffectCount + 1);
    for (std::size_t i = 0; i < kEffectCount; ++i) {
        EXPECT_EQ(sound.played[i], static_cast<EffectId>(i));
    }
    EXPECT_EQ(sound.played.back(), EffectId::GameOver);
    EXPECT_EQ(backend.present(), ApiStatus::Ok);
    EXPECT_EQ(backend.present(), ApiStatus::Ok);
    EXPECT_EQ(sound.ticks, 2);  // SR-HAL-11: once per tick
}

// ---- Shutdown (SR-HAL-07) ------------------------------------------------------------------

TEST_F(FpgaBackendTest, SR_HAL_07_ShutdownKeepsDisplayEnableAndReleasesOnce) {
    ASSERT_EQ(backend.loadTiles(distinctTiles(), assets::kPalette), ApiStatus::Ok);
    ASSERT_EQ(backend.drawText(0, 0, "SAFE TO SWITCH OFF", TextScale::X1), ApiStatus::Ok);
    ASSERT_EQ(backend.present(), ApiStatus::Ok);
    board.regs.frameCounter = 77;
    ASSERT_EQ(backend.frameCount(), 77U);
    const std::uint32_t control = board.regs.control;
    const std::size_t accesses = board.regs.log.size();

    backend.shutdown();
    backend.shutdown();
    EXPECT_EQ(board.regs.releaseCalls, 1);
    EXPECT_EQ(board.regs.control, control);  // Display Enable not cleared
    for (std::size_t i = accesses; i < board.regs.log.size(); ++i) {
        EXPECT_FALSE(isWrite(board.regs.log[i])) << "register written by shutdown";
    }
    // After shutdown nothing reaches the bridge.
    EXPECT_EQ(backend.present(), ApiStatus::BackendError);
    EXPECT_EQ(backend.loadTiles(distinctTiles(), assets::kPalette), ApiStatus::BackendError);
    board.regs.frameCounter = 99;
    EXPECT_EQ(backend.frameCount(), 77U);                // the last value read
    EXPECT_EQ(backend.setTile(0, 0, 1), ApiStatus::Ok);  // shadow only
    EXPECT_EQ(board.regs.accessesAfterRelease, 0);
}

TEST(FpgaBackendLifetime, SR_HAL_07_DestroyingTheBackendReleasesOnce) {
    Board board;
    {
        FpgaBackend backend(board.regs, board.clock, board.input, board.bindings);
        ASSERT_EQ(backend.loadTiles(distinctTiles(), assets::kPalette), ApiStatus::Ok);
    }
    EXPECT_EQ(board.regs.releaseCalls, 1);
    EXPECT_NE(board.regs.control, 0U);  // Display Enable still set
    EXPECT_EQ(board.regs.accessesAfterRelease, 0);
    {
        FpgaBackend backend(board.regs, board.clock, board.input, board.bindings);
        backend.shutdown();
    }
    EXPECT_EQ(board.regs.releaseCalls, 2);  // once per backend, not twice
}

}  // namespace
}  // namespace arcade
