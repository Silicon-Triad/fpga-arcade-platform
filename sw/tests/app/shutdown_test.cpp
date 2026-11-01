// Unit tests for the shutdown steps, signals, the closed window and screen 10b (issue #15,
// SR-FW-13, SR-FW-14, SR-HAL-07, section 3.1.1 screen 10b).
#include "engine/shutdown.hpp"

#include <gtest/gtest.h>

#include <array>
#include <csignal>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "app/startup.hpp"
#include "arcade/tables.hpp"
#include "assets/game_tiles.hpp"
#include "assets/palette.hpp"
#include "assets/text_tiles.hpp"
#include "assets/tile_ids.hpp"
#include "assets/tile_set.hpp"
#include "games/game_registry.hpp"

#ifdef ARCADE_HAS_SDL
#include <SDL2/SDL.h>

#include "backend_sdl/sdl_backend.hpp"
#endif

namespace arcade {
namespace {

using Calls = std::vector<std::string>;
using Tilemap = std::array<std::array<TileIndex, kGridCols>, kGridRows>;

constexpr int kSplashTicks = TICK_RATE;  // sw/app/startup.cpp
constexpr int kHoldTicks = 180;          // App. A SHUTDOWN_HOLD

// The steps every shutdown must run, in order (SR-FW-13, SR-FW-14).
Calls steps() {
    return {"mute", "save", "clear", "text:46:SAFE TO SWITCH OFF", "present", "release"};
}

// Records the calls that matter and keeps a true tilemap. The fake models display enable: the
// backend sets it once the tiles are loaded, and only a call to clear it could change it.
class FakeBackend final : public EngineApi {
  public:
    explicit FakeBackend(Calls& calls) : calls_(calls) {}
    ~FakeBackend() override { calls_.emplace_back("release"); }
    FakeBackend(const FakeBackend&) = delete;
    FakeBackend& operator=(const FakeBackend&) = delete;
    FakeBackend(FakeBackend&&) = delete;
    FakeBackend& operator=(FakeBackend&&) = delete;

    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        map[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = tile;
        return ApiStatus::Ok;
    }
    ApiStatus fillRect(int, int, int, int, TileIndex) noexcept override { return ApiStatus::Ok; }
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override {
        calls_.push_back("text:" + std::to_string(row) + ":" + std::string(text));
        return assets::textToTiles(
            col, row, text, scale,
            [](void* self, int c, int r, TileIndex tile) noexcept {
                static_cast<FakeBackend*>(self)->setTile(c, r, tile);
            },
            this);
    }
    void clear(TileIndex tile) noexcept override {
        calls_.emplace_back("clear");
        for (auto& line : map) {
            line.fill(tile);
        }
    }
    ApiStatus present() noexcept override {
        calls_.emplace_back("present");
        return ApiStatus::Ok;
    }
    InputFrame pollInput() noexcept override {
        ++polls;
        return script ? script(polls) : InputFrame{};
    }
    void playSound(EffectId) noexcept override {}
    void setMute(bool on) noexcept override {
        if (on) {
            calls_.emplace_back("mute");
        }
    }
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return 0; }
    ApiStatus setInputContext(InputContext) noexcept override { return ApiStatus::Ok; }

    Tilemap map{};
    int polls = 0;
    std::function<InputFrame(int poll)> script;

  private:
    Calls& calls_;
};

// The calls from the first shutdown step on, without drawing done by the game loop.
Calls fromMute(const Calls& calls) {
    for (std::size_t i = 0; i < calls.size(); ++i) {
        if (calls[i] == "mute") {
            return {calls.begin() + static_cast<std::ptrdiff_t>(i), calls.end()};
        }
    }
    return {};
}

Calls withoutSave(Calls calls) {
    calls.erase(calls.begin() + 1);
    return calls;
}

App makeApp(FakeBackend& api) {
    return {api, kGameRegistry.data(), kGameRegistry.size(), [](std::uint8_t) { return 0U; }};
}

class FinishShutdownTest : public ::testing::Test {
  protected:
    FinishShutdownTest()
        : api(std::make_unique<FakeBackend>(calls)), app(makeApp(*api)), raw(api.get()) {}

    int finish(bool confirmed) {
        return finishShutdown(
            *raw, app, confirmed, [this] { calls.emplace_back("save"); }, [this] { api.reset(); });
    }

    Calls calls;
    std::unique_ptr<FakeBackend> api;
    App app;
    FakeBackend* raw;
};

TEST_F(FinishShutdownTest, SR_FW_13_StepsRunInOrder) {
    finish(true);
    EXPECT_EQ(calls, steps());
}

TEST_F(FinishShutdownTest, SR_FW_13_ReturnsShutdownRequestedStatus) {
    EXPECT_EQ(kExitShutdownRequested, 3);
    EXPECT_EQ(finish(true), kExitShutdownRequested);
}

TEST_F(FinishShutdownTest, SR_FW_14_SameStepsAndExitZero) {
    EXPECT_EQ(finish(false), 0);
    EXPECT_EQ(calls, steps());
}

TEST_F(FinishShutdownTest, SR_FW_13_NoMessageBeforeTheSaveHasFinished) {
    app.setState(AppState::ShuttingDown);
    app.render(*raw);
    EXPECT_TRUE(calls.empty());
    app.shuttingDown().saveFinished();
    app.render(*raw);
    EXPECT_FALSE(calls.empty());
}

TEST_F(FinishShutdownTest, Screen10bGoldenTilemap) {
    // Every cell, from the numbers of section 3.1.1 screen 10b, not from ShuttingDownScreen.
    raw->map[0][0] = assets::kTileFrame;  // anything left behind must go
    Tilemap golden{};
    ASSERT_EQ(assets::textToTiles(
                  28, 46, "SAFE TO SWITCH OFF", TextScale::X4,
                  [](void* map, int c, int r, TileIndex tile) noexcept {
                      (*static_cast<Tilemap*>(
                          map))[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = tile;
                  },
                  &golden),
              ApiStatus::Ok);
    finishShutdown(*raw, app, true, [] {}, [] {});
    EXPECT_EQ(raw->map, golden);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            if (row < 46 || row > 49 || col < 28 || col > 99) {
                EXPECT_EQ(raw->map[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)],
                          assets::kFontFirstTile);
            }
        }
    }
}

TEST(Screen10b, BackgroundTileUsesOnlyPaletteEntry0) {
    const auto& gameTiles = assets::gameTilePatterns();
    const auto tiles = assets::buildTileSet(gameTiles.data(), gameTiles.size());
    if (!tiles) {
        FAIL() << "tile set did not build";
    }
    for (const std::uint8_t byte : (*tiles)[assets::kFontFirstTile]) {
        EXPECT_EQ(byte, 0U);
    }
}

// runArcade with a fake backend. `configure` sets the script and stop request.
int runFake(Calls& calls, const std::function<void(BackendStart&, FakeBackend&)>& configure,
            const std::function<bool()>& signal = {}) {
    std::FILE* log = std::tmpfile();
    const int status = runArcade(
        Options{},
        [&calls, &configure](const Options&) {
            auto api = std::make_unique<FakeBackend>(calls);
            FakeBackend& fake = *api;
            BackendStart start;
            start.api = std::move(api);
            configure(start, fake);
            return start;
        },
        log, signal);
    (void)std::fclose(log);
    return status;
}

TEST(RunArcade, SR_FW_13_HoldThenSelectExitsWithShutdownStatus) {
    Calls calls;
    const auto configure = [](BackendStart&, FakeBackend& fake) {
        fake.script = [](int poll) {
            InputFrame frame{};
            const int tick = poll - kSplashTicks;
            if (tick >= 1 && tick <= kHoldTicks + 1) {
                frame.held[static_cast<std::size_t>(Action::Back)] = true;
            }
            if (tick == 1) {
                frame.events[0] = InputEvent{InputEventType::KeyDown, Action::Back, 0};
                frame.count = 1;
            } else if (tick == kHoldTicks + 3) {
                frame.events[0] = InputEvent{InputEventType::KeyDown, Action::Select, 0};
                frame.count = 1;
            }
            return frame;
        };
    };
    EXPECT_EQ(runFake(calls, configure), kExitShutdownRequested);
    EXPECT_EQ(fromMute(calls),
              withoutSave(steps()));  // the store has nothing pending: no call here
}

TEST(RunArcade, SR_FW_13_FinalFrameIsKeptBeforeRelease) {
    Calls calls;
    constexpr int kFinalTicks = 5;
    const auto configure = [](BackendStart& start, FakeBackend& fake) {
        start.finalFrameTicks = kFinalTicks;
        fake.script = [](int poll) {
            InputFrame frame{};
            const int tick = poll - kSplashTicks;
            if (tick >= 1 && tick <= kHoldTicks + 1) {
                frame.held[static_cast<std::size_t>(Action::Back)] = true;
            }
            if (tick == 1 || tick == kHoldTicks + 3) {
                frame.events[0] = InputEvent{InputEventType::KeyDown,
                                             tick == 1 ? Action::Back : Action::Select, 0};
                frame.count = 1;
            }
            return frame;
        };
    };
    EXPECT_EQ(runFake(calls, configure), kExitShutdownRequested);
    Calls expected = withoutSave(steps());
    expected.insert(expected.end() - 1, kFinalTicks, "present");
    EXPECT_EQ(fromMute(calls), expected);
}

volatile std::sig_atomic_t gTestSignal = 0;
extern "C" void onTestSignal(int) { gTestSignal = 1; }

class SignalTest : public ::testing::TestWithParam<int> {};

TEST_P(SignalTest, SR_FW_14_SignalFlagRunsTheStepsAndExitsZero) {
    gTestSignal = 0;
    auto* const previous = std::signal(GetParam(), &onTestSignal);
    Calls calls;
    const auto configure = [](BackendStart& start, FakeBackend& fake) {
        start.finalFrameTicks = TICK_RATE;  // only a confirmed shutdown keeps the frame
        fake.script = [](int poll) {
            if (poll == kSplashTicks + 3) {
                (void)std::raise(GetParam());
            }
            return InputFrame{};
        };
    };
    const int status = runFake(calls, configure, [] { return gTestSignal != 0; });
    (void)std::signal(GetParam(), previous);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(fromMute(calls), withoutSave(steps()));
}

INSTANTIATE_TEST_SUITE_P(Signals, SignalTest, ::testing::Values(SIGINT, SIGTERM));

#ifdef ARCADE_HAS_SDL
class NoKeys final : public KeyActionLookup {
  public:
    std::optional<KeyMapping> lookup(InputContext, std::uint16_t) const noexcept override {
        return std::nullopt;
    }
};

// The real backend under a recorder, so the real SDL_QUIT path is what stops the loop.
class RecordedSdl final : public EngineApi {
  public:
    RecordedSdl(std::unique_ptr<SdlBackend> sdl, Calls& calls)
        : sdl_(std::move(sdl)), calls_(calls) {}
    ~RecordedSdl() override { calls_.emplace_back("release"); }
    RecordedSdl(const RecordedSdl&) = delete;
    RecordedSdl& operator=(const RecordedSdl&) = delete;
    RecordedSdl(RecordedSdl&&) = delete;
    RecordedSdl& operator=(RecordedSdl&&) = delete;

    ApiStatus setTile(int c, int r, TileIndex t) noexcept override {
        return sdl_->setTile(c, r, t);
    }
    ApiStatus fillRect(int c, int r, int w, int h, TileIndex t) noexcept override {
        return sdl_->fillRect(c, r, w, h, t);
    }
    ApiStatus drawText(int, int row, std::string_view text, TextScale) noexcept override {
        calls_.push_back("text:" + std::to_string(row) + ":" + std::string(text));
        return ApiStatus::Ok;
    }
    void clear(TileIndex tile) noexcept override {
        calls_.emplace_back("clear");
        sdl_->clear(tile);
    }
    ApiStatus present() noexcept override {
        calls_.emplace_back("present");
        return sdl_->present();
    }
    InputFrame pollInput() noexcept override { return sdl_->pollInput(); }
    void playSound(EffectId id) noexcept override { sdl_->playSound(id); }
    void setMute(bool on) noexcept override {
        calls_.emplace_back("mute");
        sdl_->setMute(on);
    }
    ApiStatus loadTiles(const TileSet& p, const Palette& c) noexcept override {
        return sdl_->loadTiles(p, c);
    }
    std::uint32_t frameCount() const noexcept override { return sdl_->frameCount(); }
    ApiStatus setInputContext(InputContext c) noexcept override { return sdl_->setInputContext(c); }
    const SdlBackend* sdl() const noexcept { return sdl_.get(); }

  private:
    std::unique_ptr<SdlBackend> sdl_;
    Calls& calls_;
};

class EscIsBack final : public KeyActionLookup {
  public:
    std::optional<KeyMapping> lookup(InputContext, std::uint16_t code) const noexcept override {
        if (code != linux_key::kEsc) {
            return std::nullopt;
        }
        return KeyMapping{Action::Back, '\0'};
    }
};

TEST(SdlHold, SR_UI_14_RealBackendHeldKeyOpensBoxAfter180Ticks) {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");  // CI has no display
    const EscIsBack keys;
    auto sdl = SdlBackend::open(keys);
    ASSERT_NE(sdl, nullptr) << SDL_GetError();
    App app(*sdl, kGameRegistry.data(), kGameRegistry.size(), [](std::uint8_t) { return 0U; });
    app.setState(AppState::Menu);
    SDL_Event down{};
    down.type = SDL_KEYDOWN;
    down.key.state = SDL_PRESSED;
    down.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
    ASSERT_EQ(SDL_PushEvent(&down), 1);
    app.update(sdl->pollInput());
    for (int tick = 1; tick < kHoldTicks; ++tick) {
        down.key.repeat = 1;  // auto-repeat must not restart the count (SR-INP-07)
        ASSERT_EQ(SDL_PushEvent(&down), 1);
        app.update(sdl->pollInput());
    }
    EXPECT_EQ(app.state(), AppState::Menu);
    app.update(sdl->pollInput());
    EXPECT_EQ(app.state(), AppState::ShutdownConfirm);
}

TEST(RunArcade, SR_FW_14_SdlQuitEventRunsTheSameStepsAndExitsZero) {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");  // CI has no display
    const NoKeys keys;
    Calls calls;
    std::FILE* log = std::tmpfile();
    const int status = runArcade(
        Options{},
        [&keys, &calls](const Options&) {
            BackendStart start;
            auto sdl = SdlBackend::open(keys);
            if (sdl == nullptr) {
                return start;
            }
            SDL_Event quit{};
            quit.type = SDL_QUIT;
            (void)SDL_PushEvent(&quit);
            auto recorded = std::make_unique<RecordedSdl>(std::move(sdl), calls);
            const SdlBackend* window = recorded->sdl();
            start.stopRequested = [window] { return window->quitRequested(); };
            start.finalFrameTicks = TICK_RATE;  // only a confirmed shutdown keeps the frame
            start.api = std::move(recorded);
            return start;
        },
        log);
    (void)std::fclose(log);
    EXPECT_EQ(status, 0);
    EXPECT_EQ(fromMute(calls), withoutSave(steps()));
}
#endif

}  // namespace
}  // namespace arcade
