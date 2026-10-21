// Compile-only checks of the frozen Arcade API, engine API, game interface and shared types
// (issue #3), plus the contracts of pollInput() overflow, setInputContext() and the game sound
// rule, shown on a dummy backend and a dummy game.
#include "arcade/arcade_api.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string_view>
#include <type_traits>

#include "arcade/engine_api.hpp"
#include "arcade/game.hpp"
#include "arcade/types.hpp"

namespace arcade {
namespace {

static_assert(kGridCols == 128 && kGridRows == 96, "SR-FW-06: 128 x 96 tiles");
static_assert(kTilePx == 8, "TBL-TilePattern: 8 x 8 pixels");
static_assert(sizeof(TilePattern) == kTilePatternBytes && kTilePatternBytes == 32,
              "TBL-TilePattern: 32 bytes per tile");
static_assert(std::tuple_size_v<TileSet> == 256, "TBL-TilePattern: 256 tiles");
static_assert(std::tuple_size_v<Palette> == 16, "TBL-Palette: 16 colours");
static_assert(kActionCount == 11, "TBL-KeyBinding: 11 actions");
static_assert(static_cast<int>(EffectId::Eat) == 0 && kEffectCount == 7,
              "TBL-SoundEffect: effect ids 0-6");
static_assert(kInputContextCount == 9, "TBL-KeyBinding: 9 contexts");
static_assert(std::is_abstract_v<ArcadeApi> && std::is_abstract_v<EngineApi> &&
                  std::is_abstract_v<Game>,
              "SR-FW-03, SR-FW-05: abstract interfaces");
static_assert(std::is_base_of_v<ArcadeApi, EngineApi>, "games get the engine API as ArcadeApi");

// Minimal backend: records calls; pollInput() drains a queue as the API contract requires.
class DummyApi : public EngineApi {
  public:
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        if (col < 0 || col >= kGridCols || row < 0 || row >= kGridRows) {
            return ApiStatus::OutOfRange;
        }
        lastTile = tile;
        ++drawCalls;
        return ApiStatus::Ok;
    }
    ApiStatus fillRect(int /*col*/, int /*row*/, int /*width*/, int /*height*/,
                       TileIndex /*tile*/) noexcept override {
        ++drawCalls;
        return ApiStatus::Ok;
    }
    ApiStatus drawText(int /*col*/, int /*row*/, std::string_view /*text*/,
                       TextScale /*scale*/) noexcept override {
        ++drawCalls;
        return ApiStatus::Ok;
    }
    void clear(TileIndex /*tile*/) noexcept override { ++drawCalls; }
    ApiStatus present() noexcept override {
        ++frames;
        return ApiStatus::Ok;
    }
    InputFrame pollInput() noexcept override {
        InputFrame frame;
        while (frame.count < kMaxEventsPerTick && !pending.empty()) {
            frame.events[frame.count++] = pending.front();
            pending.pop_front();
        }
        return frame;
    }
    void playSound(EffectId effectId) noexcept override {
        lastSound = effectId;
        ++soundCalls;
    }
    void setMute(bool on) noexcept override { muted = on; }
    ApiStatus loadTiles(const TileSet& /*patterns*/, const Palette& /*palette*/) noexcept override {
        return ApiStatus::Ok;
    }
    std::uint32_t frameCount() const noexcept override { return frames; }
    ApiStatus setInputContext(InputContext ctx) noexcept override {
        if (ctx == InputContext::Global) {
            return ApiStatus::InvalidArgument;
        }
        context = ctx;
        return ApiStatus::Ok;
    }

    std::deque<InputEvent> pending;
    int drawCalls = 0;
    TileIndex lastTile = 0;
    bool muted = false;
    std::uint32_t frames = 0;
    int soundCalls = 0;
    EffectId lastSound = EffectId::Eat;
    InputContext context = InputContext::Menu;
};

// Minimal game: counts Select presses as score (playing MenuSelect once each), ends on Back.
class DummyGame : public Game {
  public:
    void init() noexcept override {
        score_ = 0;
        over_ = false;
    }
    void update(const InputFrame& input) noexcept override {
        for (std::size_t i = 0; i < input.count; ++i) {
            const InputEvent& e = input.events[i];
            if (e.type == InputEventType::KeyDown && e.action == Action::Select) {
                ++score_;
                soundPending_ = true;
            }
            if (e.type == InputEventType::KeyDown && e.action == Action::Back) {
                over_ = true;
            }
        }
    }
    void render(ArcadeApi& api) noexcept override {
        api.clear(0);
        api.drawText(0, 0, "SCORE", TextScale::X1);
        if (soundPending_) {  // Sound rule in game.hpp: once, in the next render().
            api.playSound(EffectId::MenuSelect);
            soundPending_ = false;
        }
    }
    bool isOver() const noexcept override { return over_; }
    std::uint32_t score() const noexcept override { return score_; }

  private:
    std::uint32_t score_ = 0;
    bool over_ = false;
    bool soundPending_ = false;
};

TEST(ArcadeApi, SR_FW_03_DummyGameImplementsInterface) {
    DummyApi api;
    DummyGame game;
    Game& g = game;
    g.init();
    api.pending.push_back({InputEventType::KeyDown, Action::Select, 0});
    api.pending.push_back({InputEventType::KeyDown, Action::Back, 0});
    g.update(api.pollInput());
    EXPECT_EQ(g.score(), 1U);
    EXPECT_TRUE(g.isOver());
}

TEST(ArcadeApi, SR_FW_05_GameDrawsOnlyThroughApi) {
    DummyApi api;
    DummyGame game;
    ArcadeApi& a = api;
    game.render(a);
    EXPECT_EQ(a.present(), ApiStatus::Ok);
    EXPECT_EQ(api.drawCalls, 2);
    EXPECT_EQ(a.frameCount(), 1U);
    EXPECT_EQ(a.setTile(kGridCols, 0, 1), ApiStatus::OutOfRange);
    a.playSound(EffectId::MenuSelect);
    a.setMute(true);
    EXPECT_TRUE(api.muted);
}

TEST(EngineApi, SR_INP_09_SetInputContextRejectsGlobal) {
    DummyApi api;
    EngineApi& engine = api;
    EXPECT_EQ(engine.setInputContext(InputContext::Initials), ApiStatus::Ok);
    EXPECT_EQ(api.context, InputContext::Initials);
    EXPECT_EQ(engine.setInputContext(InputContext::Global), ApiStatus::InvalidArgument);
    EXPECT_EQ(api.context, InputContext::Initials);
}

TEST(Game, SR_UI_05_SoundPlayedOnceNotReplayedWithoutUpdate) {
    DummyApi api;
    DummyGame game;
    game.init();
    api.pending.push_back({InputEventType::KeyDown, Action::Select, 0});
    game.update(api.pollInput());
    game.render(api);
    EXPECT_EQ(api.soundCalls, 1);
    EXPECT_EQ(api.lastSound, EffectId::MenuSelect);
    game.render(api);  // e.g. paused: render() without update() plays nothing (SR-UI-05)
    EXPECT_EQ(api.soundCalls, 1);
}

TEST(ArcadeApi, SR_INP_02_PollInputKeepsOverflowQueuedInOrder) {
    constexpr std::size_t kSent = 40;
    DummyApi api;
    for (std::size_t i = 0; i < kSent; ++i) {
        api.pending.push_back(
            {InputEventType::KeyDown, Action::Char, static_cast<char>('A' + (i % 26))});
    }
    const InputFrame first = api.pollInput();
    ASSERT_EQ(first.count, kMaxEventsPerTick);
    const InputFrame second = api.pollInput();
    ASSERT_EQ(second.count, kSent - kMaxEventsPerTick);
    for (std::size_t i = 0; i < kSent; ++i) {
        const InputEvent& e =
            i < kMaxEventsPerTick ? first.events[i] : second.events[i - kMaxEventsPerTick];
        EXPECT_EQ(e.ch, static_cast<char>('A' + (i % 26))) << "event " << i;
    }
    EXPECT_EQ(api.pollInput().count, 0U);
}

}  // namespace
}  // namespace arcade
