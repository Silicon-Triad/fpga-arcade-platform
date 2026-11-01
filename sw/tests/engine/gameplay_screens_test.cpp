// Unit tests for the gameplay screens (issue #10, SR-UI-04 to SR-UI-09).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "assets/tile_ids.hpp"
#include "engine/app.hpp"
#include "engine/ui_layout.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

class ScriptedGame final : public Game {
  public:
    void init() noexcept override {
        ++inits;
        ticks = 0;
        over = false;
    }
    void update(const InputFrame&) noexcept override { ++ticks; }
    void render(ArcadeApi&) noexcept override {}
    bool isOver() const noexcept override { return over; }
    std::uint32_t score() const noexcept override { return points; }
    int inits = 0;
    int ticks = 0;
    bool over = false;
    std::uint32_t points = 120;
};

ScriptedGame* lastGame = nullptr;
std::unique_ptr<Game> makeGame() {
    auto game = std::make_unique<ScriptedGame>();
    lastGame = game.get();
    return game;
}

constexpr std::array<GameRegistryRow, 1> kSnakeRow{{{kGameIdSnake, "SNAKE", 0, &makeGame}}};
constexpr std::array<GameRegistryRow, 1> kBreakoutRow{
    {{kGameIdBreakout, "BREAKOUT", 0, &makeGame}}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame press(Action action) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    frame.count = 1;
    return frame;
}

bool drew(const FakeEngineApi& api, const std::string& entry) {
    return std::find(api.placed.begin(), api.placed.end(), entry) != api.placed.end();
}

class GameplayTest : public ::testing::Test {
  protected:
    GameplayTest() : app(api, kSnakeRow.data(), kSnakeRow.size(), &noScore, &scores) {
        game = lastGame;
        app.setState(AppState::Menu);
        app.update(press(Action::Select));
    }
    // Fills the top 10 with `score`.
    void fillTable(std::uint32_t score) {
        for (std::size_t i = 0; i < HISCORE_SLOTS; ++i) {
            ASSERT_TRUE(scores.insert(kGameIdSnake, {'A', 'A', 'A'}, score).has_value());
        }
    }
    FakeEngineApi api;
    HighScoreStore scores;
    App app;
    ScriptedGame* game;
};

TEST_F(GameplayTest, SR_UI_05_NoUpdatesAcrossSixtySecondPause) {
    constexpr int kPauseTicks = 3600;  // 60 s at 60 ticks per second
    app.update(InputFrame{});
    const int ticks = game->ticks;
    const std::uint32_t points = game->score();
    app.update(press(Action::Pause));
    ASSERT_EQ(app.state(), AppState::Paused);
    for (int i = 0; i < kPauseTicks; ++i) {
        app.update(InputFrame{});
        app.render(api);
    }
    EXPECT_EQ(game->ticks, ticks);
    EXPECT_EQ(game->score(), points);
    EXPECT_EQ(game->inits, 1);
}

TEST_F(GameplayTest, SR_UI_05_PausedBoxMatchesScreen5) {
    app.update(press(Action::Pause));
    app.render(api);
    EXPECT_EQ(api.cells[38][40], assets::kTileBoxBlue);
    EXPECT_EQ(api.cells[57][87], assets::kTileBoxBlue);
    EXPECT_NE(api.cells[58][88], assets::kTileBoxBlue);
    EXPECT_TRUE(drew(api, "52,41:PAUSED"));
    EXPECT_TRUE(drew(api, "60,48:P RESUME"));
    EXPECT_TRUE(drew(api, "56,50:ESC QUIT TO MENU"));
    EXPECT_TRUE(drew(api, "57,52:M SOUND ON/OFF"));
}

TEST_F(GameplayTest, SR_UI_06_ResumeContinuesFromPausedState) {
    app.update(InputFrame{});
    const int ticks = game->ticks;
    app.update(press(Action::Pause));
    app.update(press(Action::Pause));
    EXPECT_EQ(app.state(), AppState::Playing);
    EXPECT_EQ(game->ticks, ticks);
    EXPECT_EQ(game->inits, 1);
    app.update(InputFrame{});
    EXPECT_EQ(game->ticks, ticks + 1);
}

TEST_F(GameplayTest, SR_UI_07_BackFromPlayingReachesMenuWithoutSaving) {
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(scores.table(kGameIdSnake).count, 0U);
}

TEST_F(GameplayTest, SR_UI_07_BackFromPausedReachesMenuWithoutSaving) {
    app.update(press(Action::Pause));
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(scores.table(kGameIdSnake).count, 0U);
}

TEST_F(GameplayTest, SR_UI_08_QualifyingScoreGoesToInitials) {
    game->over = true;
    app.update(InputFrame{});
    EXPECT_EQ(app.state(), AppState::Initials);
}

TEST_F(GameplayTest, SR_UI_08_SelectLeavesInitialsPlaceholder) {
    game->over = true;
    app.update(InputFrame{});
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(GameplayTest, SR_UI_08_NonQualifyingScoreGoesToGameOver) {
    fillTable(500);
    game->over = true;
    app.update(InputFrame{});
    EXPECT_EQ(app.state(), AppState::GameOver);
}

class GameOverTest : public GameplayTest {
  protected:
    void SetUp() override {
        fillTable(560);
        game->points = 340;
        game->over = true;
        app.update(InputFrame{});
        ASSERT_EQ(app.state(), AppState::GameOver);
    }
};

TEST_F(GameOverTest, SR_UI_09_SelectRestartsSameGame) {
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Playing);
    EXPECT_EQ(game->inits, 2);
    EXPECT_FALSE(game->isOver());
}

TEST_F(GameOverTest, SR_UI_09_BackGoesToMenu) {
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(GameOverTest, SR_UI_09_BoxMatchesScreen6) {
    app.render(api);
    EXPECT_EQ(api.cells[34][36], assets::kTileBoxRed);
    EXPECT_EQ(api.cells[61][91], assets::kTileBoxRed);
    EXPECT_NE(api.cells[62][92], assets::kTileBoxRed);
    EXPECT_TRUE(drew(api, "46,37:GAME OVER"));
    EXPECT_TRUE(drew(api, "58,45:SCORE 000340"));
    EXPECT_TRUE(drew(api, "59,47:HI 000560"));
    EXPECT_TRUE(drew(api, "56,52:ENTER PLAY AGAIN"));
    EXPECT_TRUE(drew(api, "57,54:ESC MAIN MENU"));
    EXPECT_TRUE(std::any_of(api.placed.begin(), api.placed.end(), [](const std::string& entry) {
        return entry.find(",94:") != std::string::npos &&
               entry.find("ENTER PLAY AGAIN  ESC MENU  M SOUND: ON") != std::string::npos;
    }));
}

TEST_F(GameplayTest, SR_UI_04_SnakeHudMatchesScreen3) {
    ASSERT_TRUE(scores.insert(kGameIdSnake, {'A', 'B', 'C'}, 560).has_value());
    app.render(api);
    EXPECT_TRUE(drew(api, "2,1:SNAKE"));
    EXPECT_TRUE(drew(api, "58,1:SCORE 000120"));
    EXPECT_TRUE(drew(api, "117,1:HI 000560"));
}

TEST_F(GameplayTest, SR_UI_04_HudUpdatesEveryTick) {
    app.render(api);
    game->points = 130;
    app.update(InputFrame{});
    app.render(api);
    EXPECT_TRUE(drew(api, "58,1:SCORE 000130"));
}

TEST(GameplayHud, SR_UI_04_ScoreClampsToSixDigits) {
    EXPECT_EQ(scoreLine("SCORE", SCORE_MAX + 1).text(), "SCORE 999999");
}

TEST(GameplayHud, SR_UI_04_BreakoutHudMatchesScreen4LeftAndCentre) {
    FakeEngineApi api;
    App app(api, kBreakoutRow.data(), kBreakoutRow.size(), &noScore);
    app.setState(AppState::Menu);
    app.update(press(Action::Select));
    app.render(api);
    EXPECT_TRUE(drew(api, "2,1:BREAKOUT"));
    EXPECT_TRUE(drew(api, "58,1:SCORE 000120"));
}

}  // namespace
}  // namespace arcade
