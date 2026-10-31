// Unit tests for the menu and app state machine (issue #8, SR-FW-03, SR-FW-04, SR-UI-01 to
// SR-UI-03, QA-02).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>

#include "engine/app.hpp"
#include "engine/game_loop.hpp"
#include "engine/preview_api.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

constexpr TileIndex kGameTile = 200;

// QA-02: this class and one row below are all a third game needs.
class TestGame final : public Game {
  public:
    void init() noexcept override {
        ++inits;
        updates = 0;
    }
    void update(const InputFrame&) noexcept override { ++updates; }
    void render(ArcadeApi& api) noexcept override {
        api.fillRect(0, 0, kGridCols, kGridRows, kGameTile);
        api.playSound(EffectId::Eat);
    }
    bool isOver() const noexcept override { return false; }
    std::uint32_t score() const noexcept override { return 0; }
    int inits = 0;
    int updates = 0;
};

TestGame* lastGame = nullptr;
std::unique_ptr<Game> makeTestGame() {
    auto game = std::make_unique<TestGame>();
    lastGame = game.get();
    return game;
}

// Deliberately out of menu_order.
constexpr std::array<GameRegistryRow, 3> kRows{{
    {7, "THIRD", 2, &makeTestGame},
    {kGameIdSnake, "SNAKE", 0, &makeTestGame},
    {kGameIdBreakout, "BREAKOUT", 1, &makeTestGame},
}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame press(Action action) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    frame.count = 1;
    return frame;
}

class MenuTest : public ::testing::Test {
  protected:
    MenuTest() : app(api, kRows.data(), kRows.size(), &noScore) { app.setState(AppState::Menu); }
    FakeEngineApi api;
    App app;
};

TEST_F(MenuTest, SR_UI_01_ListsRegistryThenScores) {
    ASSERT_EQ(app.menu().entryCount(), kRows.size() + 1);
    EXPECT_EQ(app.menu().entryName(kRows.size()), "SCORES");
}

TEST_F(MenuTest, SR_FW_04_OrderFollowsMenuOrder) {
    EXPECT_EQ(app.menu().entryName(0), "SNAKE");
    EXPECT_EQ(app.menu().entryName(1), "BREAKOUT");
}

TEST_F(MenuTest, SR_UI_01_WrapsBothEnds) {
    app.update(press(Action::Up));
    EXPECT_EQ(app.menu().selection(), kRows.size());
    app.update(press(Action::Down));
    EXPECT_EQ(app.menu().selection(), 0U);
}

TEST_F(MenuTest, SR_UI_03_SelectStartsGameWithinOneTick) {
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Playing);
    EXPECT_NE(app.playing().game(), nullptr);
    EXPECT_EQ(api.contexts.back(), InputContext::Snake);
}

TEST_F(MenuTest, SR_UI_01_SelectOnScoresOpensHighScores) {
    app.update(press(Action::Up));
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::HighScores);
    EXPECT_EQ(api.contexts.back(), InputContext::HighScores);
}

TEST_F(MenuTest, QA_02_ThirdGameNeedsOneClassAndOneRow) {
    app.update(press(Action::Down));
    app.update(press(Action::Down));
    ASSERT_NE(app.menu().selectedGame(), nullptr);
    EXPECT_EQ(app.menu().selectedGame()->row->name, "THIRD");
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Playing);
}

TEST_F(MenuTest, SR_UI_02_PreviewStaysInPanelAndIsSilent) {
    app.update(InputFrame{});
    app.render(api);
    EXPECT_EQ(api.sounds, 0);
    for (int row = 0; row < kGridRows; ++row) {
        for (int col = 0; col < kGridCols; ++col) {
            const bool inPanel = col >= kPreviewFirstCol && col <= kPreviewLastCol &&
                                 row >= kPreviewFirstRow && row <= kPreviewLastRow;
            const auto r = static_cast<std::size_t>(row);
            const auto c = static_cast<std::size_t>(col);
            EXPECT_EQ(api.cells[r][c] == kGameTile, inPanel) << col << "," << row;
        }
    }
}

TEST_F(MenuTest, SR_UI_02_PreviewAnimatesSelectedGame) {
    const int before = lastGame->updates;  // lastGame is BREAKOUT: its row was built last
    app.update(press(Action::Down));
    EXPECT_EQ(app.menu().entryName(app.menu().selection()), "BREAKOUT");
    EXPECT_EQ(lastGame->updates, before + 1);
    app.update(press(Action::Down));  // THIRD: BREAKOUT stops
    EXPECT_EQ(lastGame->updates, before + 1);
}

TEST_F(MenuTest, SR_UI_02_ShowsBestScoreCaption) {
    app.render(api);
    const auto found = std::find(api.texts.begin(), api.texts.end(), "74:BEST 000000 - SNAKE");
    EXPECT_NE(found, api.texts.end());
}

class ZeroClock final : public TickClock {
  public:
    std::uint64_t nowUs() noexcept override { return 0; }
};

class BackFrom : public ::testing::TestWithParam<AppState> {};

TEST_P(BackFrom, SR_UI_07_SR_UI_12_BackReturnsToMenu) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(GetParam());
    app.update(InputFrame{});
    EXPECT_EQ(app.state(), GetParam());
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(api.contexts.back(), InputContext::Menu);
}

INSTANTIATE_TEST_SUITE_P(Placeholders, BackFrom,
                         ::testing::Values(AppState::Playing, AppState::Paused, AppState::GameOver,
                                           AppState::HighScores, AppState::Initials,
                                           AppState::KeyboardLost, AppState::ShutdownConfirm));

TEST(App, SR_UI_14_ShuttingDownIgnoresBack) {  // section 3.1.1 screen 10b: no keys work
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(AppState::ShuttingDown);
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::ShuttingDown);
}

TEST(App, SR_UI_12_ScoresThenBackReturnsToMenuWithLoopTicking) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(AppState::Menu);
    ZeroClock clock;
    GameLoop loop(api, clock);
    loop.setScreen(&app);
    api.nextInput = press(Action::Up);  // wraps to SCORES
    loop.runTick();
    api.nextInput = press(Action::Select);
    loop.runTick();
    ASSERT_EQ(app.state(), AppState::HighScores);
    for (int i = 0; i < 10; ++i) {  // the loop keeps polling and presenting on the placeholder
        loop.runTick();
    }
    EXPECT_EQ(api.polls, 12);
    EXPECT_EQ(api.presents, 12);
    api.nextInput = press(Action::Back);
    loop.runTick();
    EXPECT_EQ(app.state(), AppState::Menu);
    loop.runTick();
    EXPECT_EQ(api.presents, 14);
}

TEST(App, SR_FW_01_BootGoesToMenuOnlyWhenStartupIsDone) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    for (int i = 0; i < 5; ++i) {
        app.update(press(Action::Select));
        app.update(press(Action::Back));
    }
    EXPECT_EQ(app.state(), AppState::Boot);
    app.bootFinished();
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(api.contexts.back(), InputContext::Menu);
}

TEST(App, SR_FW_01_BootFinishedLeavesOtherStatesAlone) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(AppState::Paused);
    app.bootFinished();
    EXPECT_EQ(app.state(), AppState::Paused);
}

TEST(App, StartsInBootAndEveryStateHasAScreen) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    EXPECT_EQ(app.state(), AppState::Boot);
    std::array<Screen*, kAppStateCount> screens{};
    for (std::size_t i = 0; i < kAppStateCount; ++i) {
        screens[i] = app.screenFor(static_cast<AppState>(i));
        ASSERT_NE(screens[i], nullptr);
    }
    std::sort(screens.begin(), screens.end());
    EXPECT_EQ(std::adjacent_find(screens.begin(), screens.end()), screens.end());
}

TEST(App, SR_INP_09_SetsContextOnEachChange) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(AppState::Paused);
    EXPECT_EQ(api.contexts.back(), InputContext::Paused);
    app.setState(AppState::GameOver);
    EXPECT_EQ(api.contexts.back(), InputContext::GameOver);
}

TEST(App, SR_FW_03_EngineUsesOnlyGameInterface) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(AppState::Menu);
    app.update(press(Action::Select));
    const int inits = 1;
    const auto* game = static_cast<const TestGame*>(app.playing().game());
    EXPECT_EQ(game->inits, inits);
    const int updates = game->updates;
    app.update(InputFrame{});
    EXPECT_EQ(game->updates, updates + 1);
}

}  // namespace
}  // namespace arcade
