// Unit tests for keyboard-loss handling (issue #13, SR-UI-11, SR-UI-15, SF-03, section 3.1.1
// screen 9). The SDL backend always has a keyboard, so a fake sends the events.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string_view>

#include "assets/tile_ids.hpp"
#include "engine/app.hpp"
#include "engine/ui_layout.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

constexpr std::uint32_t kPoints = 120;
constexpr int kCycles = 20;  // SF-03
constexpr std::string_view kNotice = "90:NO KEYBOARD - PLUG IN A USB KEYBOARD";

class ScriptedGame final : public Game {
  public:
    void init() noexcept override {
        ++inits;
        ticks = 0;
        points = kPoints;
    }
    void update(const InputFrame&) noexcept override {
        ++ticks;
        points += 10;
    }
    void render(ArcadeApi&) noexcept override {}
    bool isOver() const noexcept override { return over; }
    std::uint32_t score() const noexcept override { return points; }
    int inits = 0;
    int ticks = 0;
    bool over = false;
    std::uint32_t points = kPoints;
};

ScriptedGame* lastGame = nullptr;
std::unique_ptr<Game> makeGame() {
    auto game = std::make_unique<ScriptedGame>();
    lastGame = game.get();
    return game;
}

constexpr std::array<GameRegistryRow, 1> kRows{{{kGameIdSnake, "SNAKE", 0, &makeGame}}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame press(Action action) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    frame.count = 1;
    return frame;
}

InputFrame event(InputEventType type) {
    InputFrame frame{};
    frame.events[0].type = type;
    frame.count = 1;
    return frame;
}

InputFrame lost() { return event(InputEventType::KeyboardLost); }
InputFrame found() { return event(InputEventType::KeyboardFound); }

bool showed(const FakeEngineApi& api, std::string_view text) {
    return std::find(api.texts.begin(), api.texts.end(), text) != api.texts.end();
}

class KeyboardLossTest : public ::testing::Test {
  protected:
    KeyboardLossTest() : app(api, kRows.data(), kRows.size(), &noScore) {
        game = lastGame;
        app.setState(AppState::Menu);
        app.update(press(Action::Select));
        app.update(InputFrame{});
    }
    // Everything of the game an unplug must not change.
    void expectFrozen(int ticks, std::uint32_t points) const {
        EXPECT_EQ(game->ticks, ticks);
        EXPECT_EQ(game->score(), points);
        EXPECT_EQ(game->inits, 1);
    }
    FakeEngineApi api;
    App app;
    ScriptedGame* game;
};

TEST_F(KeyboardLossTest, SR_UI_11_UnplugReplugAndPauseResumesIdenticalGame) {
    const int ticks = game->ticks;
    const std::uint32_t points = game->score();

    app.update(lost());  // the same tick
    EXPECT_EQ(app.state(), AppState::KeyboardLost);
    expectFrozen(ticks, points);
    for (int i = 0; i < TICK_RATE; ++i) {
        app.update(InputFrame{});
        app.render(api);
    }
    expectFrozen(ticks, points);

    app.update(found());
    EXPECT_EQ(app.state(), AppState::Paused);
    EXPECT_EQ(api.contexts.back(), InputContext::Paused);
    app.update(InputFrame{});
    EXPECT_EQ(app.state(), AppState::Paused);  // stays paused until Pause
    expectFrozen(ticks, points);

    app.update(press(Action::Pause));
    EXPECT_EQ(app.state(), AppState::Playing);
    expectFrozen(ticks, points);
    app.update(InputFrame{});
    EXPECT_EQ(game->ticks, ticks + 1);  // continues from where it stopped
}

TEST_F(KeyboardLossTest, SR_UI_11_UnplugWhilePausedGoesToScreen9ThenBackToPaused) {
    app.update(press(Action::Pause));
    app.update(lost());
    EXPECT_EQ(app.state(), AppState::KeyboardLost);
    app.update(found());
    EXPECT_EQ(app.state(), AppState::Paused);
}

TEST_F(KeyboardLossTest, SR_UI_11_KeysBeforeTheUnplugInTheSameTickAreNotActedOn) {
    InputFrame frame = press(Action::Back);
    frame.events[1].type = InputEventType::KeyboardLost;
    frame.count = 2;
    app.update(frame);
    EXPECT_EQ(app.state(), AppState::KeyboardLost);
}

TEST_F(KeyboardLossTest, SR_UI_11_MuteBeforeTheUnplugInTheSameTickIsNotActedOn) {
    InputFrame frame = press(Action::Mute);
    frame.events[1].type = InputEventType::KeyboardLost;
    frame.count = 2;
    app.update(frame);
    EXPECT_TRUE(app.soundOn());
    EXPECT_TRUE(api.mutes.empty());
}

TEST_F(KeyboardLossTest, SR_UI_11_SecondKeyboardInTheSameTickStaysPaused) {
    InputFrame frame = lost();
    frame.events[1].type = InputEventType::KeyboardFound;
    frame.count = 2;
    app.update(frame);
    EXPECT_EQ(app.state(), AppState::Paused);
    EXPECT_FALSE(app.keyboardMissing());
}

TEST_F(KeyboardLossTest, SR_UI_11_NoKeysWorkOnScreen9) {
    app.update(lost());
    for (const Action action : {Action::Back, Action::Pause, Action::Mute, Action::Select}) {
        app.update(press(action));
        EXPECT_EQ(app.state(), AppState::KeyboardLost);
    }
    EXPECT_TRUE(app.soundOn());
}

TEST_F(KeyboardLossTest, SF_03_TwentyCyclesDuringPlayKeepTheScore) {
    const int ticks = game->ticks;
    const std::uint32_t points = game->score();
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        app.update(lost());
        ASSERT_EQ(app.state(), AppState::KeyboardLost) << cycle;
        app.render(api);
        app.update(found());
        ASSERT_EQ(app.state(), AppState::Paused) << cycle;
        app.render(api);
        app.update(press(Action::Pause));
        ASSERT_EQ(app.state(), AppState::Playing) << cycle;
        app.render(api);
    }
    expectFrozen(ticks, points);
}

TEST_F(KeyboardLossTest, Screen9_GoldenTilemap) {
    app.update(lost());
    app.render(api);

    // The picture under the box: Playing's, with an empty hint bar.
    FakeEngineApi golden;
    golden.clear(0);
    ASSERT_EQ(drawFrame(golden), ApiStatus::Ok);
    ASSERT_EQ(golden.drawText(2, 1, "SNAKE", TextScale::X1), ApiStatus::Ok);
    ASSERT_EQ(golden.drawText(58, 1, "SCORE 000130", TextScale::X1), ApiStatus::Ok);
    ASSERT_EQ(golden.drawText(117, 1, "HI 000000", TextScale::X1), ApiStatus::Ok);
    // Section 3.1.1 screen 9, written out rather than taken from the code under test.
    for (std::size_t row = 38; row <= 57; ++row) {
        for (std::size_t col = 30; col <= 97; ++col) {
            const bool edge = row == 38 || row == 57 || col == 30 || col == 97;
            golden.cells[row][col] = edge ? assets::kTileBoxRed : TileIndex{0};
        }
    }
    ASSERT_EQ(golden.drawText(42, 41, "NO KEYBOARD", TextScale::X4), ApiStatus::Ok);
    ASSERT_EQ(golden.drawText(47, 49, "PLUG IN A USB KEYBOARD TO CONTINUE", TextScale::X1),
              ApiStatus::Ok);
    ASSERT_EQ(golden.drawText(58, 52, "GAME PAUSED", TextScale::X1), ApiStatus::Ok);
    EXPECT_EQ(api.cells, golden.cells);
}

class KeepsScreen : public ::testing::TestWithParam<AppState> {};

TEST_P(KeepsScreen, SR_UI_15_StaysAndShowsNoticeUntilFound) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(GetParam());
    app.update(lost());
    EXPECT_EQ(app.state(), GetParam());
    api.clear(0);
    app.render(api);
    EXPECT_TRUE(showed(api, kNotice));

    app.update(InputFrame{});
    EXPECT_EQ(app.state(), GetParam());

    app.update(found());
    EXPECT_EQ(app.state(), GetParam());
    api.clear(0);
    app.render(api);
    EXPECT_FALSE(showed(api, kNotice));
}

TEST_P(KeepsScreen, SF_03_TwentyCyclesKeepTheScreen) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(GetParam());
    for (int cycle = 0; cycle < kCycles; ++cycle) {
        app.update(lost());
        app.render(api);
        app.update(found());
        app.render(api);
        ASSERT_EQ(app.state(), GetParam()) << cycle;
    }
}

INSTANTIATE_TEST_SUITE_P(NonPlay, KeepsScreen,
                         ::testing::Values(AppState::Menu, AppState::GameOver, AppState::Initials,
                                           AppState::HighScores));

TEST(App, SR_INP_16_KeyboardMissingAtBootKeepsNoticeOnMenuUntilFound) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setKeyboardMissing(true);
    app.render(api);
    EXPECT_TRUE(showed(api, "60:NO KEYBOARD FOUND - PLUG IN A USB KEYBOARD"));
    app.bootFinished();
    app.render(api);
    EXPECT_TRUE(showed(api, kNotice));
    app.update(found());
    app.render(api);
    EXPECT_FALSE(showed(api, kNotice));
}

TEST(App, SR_INP_16_KeyboardFoundDuringBootRemovesRow60) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setKeyboardMissing(true);
    app.update(found());
    app.render(api);
    EXPECT_FALSE(showed(api, "60:NO KEYBOARD FOUND - PLUG IN A USB KEYBOARD"));
}

}  // namespace
}  // namespace arcade
