// Unit tests for Snake (issue #18): SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18, SF-01.
#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "games/snake/snake_game.hpp"

namespace arcade {
namespace {

constexpr SnakeTiles kTiles{1, 2, 3, 4, 5};  // empty, wall, body, head, food
constexpr std::uint32_t kSeed = 1234;

/// Fake Arcade API: keeps a tilemap and records played sounds.
class FakeApi final : public ArcadeApi {
  public:
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        return fillRect(col, row, 1, 1, tile);
    }
    ApiStatus fillRect(int col, int row, int w, int h, TileIndex tile) noexcept override {
        if (col < 0 || row < 0 || w < 0 || h < 0 || col + w > kGridCols || row + h > kGridRows) {
            return ApiStatus::OutOfRange;
        }
        for (int y = row; y < row + h; ++y) {
            for (int x = col; x < col + w; ++x) {
                map[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = tile;
            }
        }
        return ApiStatus::Ok;
    }
    ApiStatus drawText(int, int, std::string_view, TextScale) noexcept override {
        return ApiStatus::Ok;
    }
    void clear(TileIndex tile) noexcept override {
        for (auto& r : map) {
            r.fill(tile);
        }
    }
    ApiStatus present() noexcept override { return ApiStatus::Ok; }
    InputFrame pollInput() noexcept override { return InputFrame{}; }
    void playSound(EffectId id) noexcept override { sounds.push_back(id); }
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return 0; }

    TileIndex at(int col, int row) const {
        return map[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
    }

    std::array<std::array<TileIndex, kGridCols>, kGridRows> map{};
    std::vector<EffectId> sounds;
};

InputFrame keys(std::initializer_list<Action> actions) {
    InputFrame f;
    for (Action a : actions) {
        f.events[f.count++] = InputEvent{InputEventType::KeyDown, a, 0};
    }
    return f;
}

void tick(SnakeGame& g, int n = 1) {
    for (int i = 0; i < n; ++i) {
        g.update(InputFrame{});
    }
}

/// Starts the game and runs until the head has made exactly one move.
void startAndMove(SnakeGame& g, Action a = Action::Right) {
    g.update(keys({a}));
    tick(g, g.stepTicks() - 1);
}

TEST(Snake, SR_GAM_02_StartsReadyLength4CentreHeadingRight) {
    SnakeGame g(kTiles, kSeed);
    EXPECT_EQ(g.state(), SnakeState::Ready);
    ASSERT_EQ(g.length(), 4U);
    EXPECT_EQ(g.cell(0), (SnakeCell{21, 15}));
    EXPECT_EQ(g.cell(3), (SnakeCell{18, 15}));
    EXPECT_EQ(g.direction(), SnakeDir::Right);
    EXPECT_EQ(g.score(), 0U);
    EXPECT_FALSE(g.isOver());
}

TEST(Snake, SR_GAM_02_WaitsInReadyUntilArrowKey) {
    SnakeGame g(kTiles, kSeed);
    tick(g, 100);
    g.update(keys({Action::Pause, Action::Select}));
    EXPECT_EQ(g.state(), SnakeState::Ready);
    EXPECT_EQ(g.cell(0), (SnakeCell{21, 15}));
    g.update(keys({Action::Up}));
    EXPECT_EQ(g.state(), SnakeState::Running);
}

TEST(Snake, SR_GAM_03_MovesOneCellEveryStepTicks) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    g.update(keys({Action::Right}));
    tick(g, SNAKE_STEP_START - 2);
    EXPECT_EQ(g.cell(0), (SnakeCell{21, 15}));
    tick(g);
    EXPECT_EQ(g.cell(0), (SnakeCell{22, 15}));
    tick(g, SNAKE_STEP_START);
    EXPECT_EQ(g.cell(0), (SnakeCell{23, 15}));
}

/// Puts food on the next cell in the current direction and runs until it is eaten.
void eatAhead(SnakeGame& g) {
    SnakeCell ahead = g.cell(0);
    switch (g.direction()) {
        case SnakeDir::Up:
            --ahead.y;
            break;
        case SnakeDir::Down:
            ++ahead.y;
            break;
        case SnakeDir::Left:
            --ahead.x;
            break;
        case SnakeDir::Right:
            ++ahead.x;
            break;
    }
    ASSERT_TRUE(g.setFood(ahead));
    const std::uint32_t before = g.score();
    while (g.score() == before) {
        tick(g);
    }
}

TEST(Snake, SR_GAM_03_SpeedRisesEveryFiveFoodToThree) {
    SnakeGame g(kTiles, kSeed);
    startAndMove(g, Action::Up);  // head at (21, 14), heading up
    std::vector<int> steps;
    for (int eaten = 1; eaten <= 14; ++eaten) {  // rows 13 .. 0
        eatAhead(g);
        steps.push_back(g.stepTicks());
    }
    startAndMove(g, Action::Right);               // turn along row 0
    for (int eaten = 15; eaten <= 20; ++eaten) {  // along row 0
        eatAhead(g);
        steps.push_back(g.stepTicks());
    }
    ASSERT_EQ(steps.size(), 20U);
    EXPECT_EQ(steps[3], 6);   // 4 eaten
    EXPECT_EQ(steps[4], 5);   // 5 eaten
    EXPECT_EQ(steps[8], 5);   // 9 eaten
    EXPECT_EQ(steps[9], 4);   // 10 eaten
    EXPECT_EQ(steps[14], 3);  // 15 eaten: the minimum
    EXPECT_EQ(steps[19], 3);  // 20 eaten: stays at the minimum
    EXPECT_EQ(g.score(), 20 * FOOD_POINTS);
    EXPECT_EQ(g.length(), SNAKE_START_LEN + 20);
}

TEST(Snake, SR_GAM_04_ReverseKeyIgnored) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    startAndMove(g);
    g.update(keys({Action::Left}));
    tick(g, g.stepTicks() - 1);
    EXPECT_EQ(g.direction(), SnakeDir::Right);
    EXPECT_EQ(g.cell(0), (SnakeCell{23, 15}));
    EXPECT_EQ(g.state(), SnakeState::Running);
}

TEST(Snake, SR_GAM_04_ReverseKeyIgnoredInReady) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    startAndMove(g, Action::Left);
    EXPECT_EQ(g.direction(), SnakeDir::Right);
    EXPECT_EQ(g.cell(0), (SnakeCell{22, 15}));
}

TEST(Snake, SR_GAM_04_OneTurnPerMoveUpThenLeft) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    startAndMove(g);
    // Up then Left within one move: only Up applies, so the snake cannot reverse into itself.
    g.update(keys({Action::Up, Action::Left}));
    tick(g, g.stepTicks() - 1);
    EXPECT_EQ(g.direction(), SnakeDir::Up);
    EXPECT_EQ(g.cell(0), (SnakeCell{22, 14}));
    EXPECT_EQ(g.state(), SnakeState::Running);
    // The second key was not kept for the next move either.
    tick(g, g.stepTicks());
    EXPECT_EQ(g.cell(0), (SnakeCell{22, 13}));
}

TEST(Snake, SR_GAM_05_FoodNeverOnSnake) {
    for (std::uint32_t seed = 0; seed < 200; ++seed) {
        SnakeGame g(kTiles, seed);
        const SnakeCell f = g.food();
        EXPECT_TRUE(f.x >= 0 && f.x < kSnakeBoardCols && f.y >= 0 && f.y < kSnakeBoardRows);
        for (std::size_t i = 0; i < g.length(); ++i) {
            EXPECT_FALSE(g.cell(i) == f) << "seed " << seed;
        }
    }
    SnakeGame g(kTiles, kSeed);
    EXPECT_FALSE(g.setFood({21, 15}));
    EXPECT_FALSE(g.setFood({kSnakeBoardCols, 0}));
}

TEST(Snake, SR_GAM_05_EatGrowsByOneAndAdds10) {
    SnakeGame g(kTiles, kSeed);
    ASSERT_TRUE(g.setFood({22, 15}));
    startAndMove(g);
    EXPECT_EQ(g.length(), 5U);
    EXPECT_EQ(g.score(), FOOD_POINTS);
    EXPECT_EQ(g.cell(0), (SnakeCell{22, 15}));
    EXPECT_EQ(g.cell(4), (SnakeCell{18, 15}));  // the tail stayed
    // New food is placed off the snake.
    for (std::size_t i = 0; i < g.length(); ++i) {
        EXPECT_FALSE(g.cell(i) == g.food());
    }
}

TEST(Snake, SR_GAM_06_WallDeath) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    g.update(keys({Action::Right}));
    // Head at x = 21; 20 moves reach x = 41, the 21st hits the wall.
    tick(g, SNAKE_STEP_START * 21 - 1);
    EXPECT_EQ(g.state(), SnakeState::Dead);
    EXPECT_EQ(g.cell(0), (SnakeCell{41, 15}));
    EXPECT_FALSE(g.isOver());
}

TEST(Snake, SR_GAM_06_SelfDeath) {
    SnakeGame g(kTiles, kSeed);
    // Grow to 5 so the snake can turn into its own body.
    ASSERT_TRUE(g.setFood({22, 15}));
    startAndMove(g);
    ASSERT_EQ(g.length(), 5U);
    g.setFood({0, 0});
    const std::array<Action, 3> turns{Action::Up, Action::Left, Action::Down};
    for (Action a : turns) {
        g.update(keys({a}));
        tick(g, g.stepTicks() - 1);
    }
    EXPECT_EQ(g.state(), SnakeState::Dead);
}

TEST(Snake, SR_GAM_06_TailCellIsFreeWhenNotEating) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    startAndMove(g);  // length 4: head 22, tail 19
    const std::array<Action, 3> turns{Action::Up, Action::Left, Action::Down};
    for (Action a : turns) {
        g.update(keys({a}));
        tick(g, g.stepTicks() - 1);
    }
    // Length 4 in a 2 x 2 loop: the head enters the cell the tail is leaving.
    EXPECT_EQ(g.state(), SnakeState::Running);
}

TEST(Snake, SR_GAM_06_SF_01_StillFrameFor30TicksThenOver) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    g.update(keys({Action::Right}));
    tick(g, SNAKE_STEP_START * 21 - 1);
    ASSERT_EQ(g.state(), SnakeState::Dead);
    FakeApi first;
    g.render(first);
    for (int i = 1; i < SNAKE_DEATH_TICKS; ++i) {
        g.update(keys({Action::Up}));  // input changes nothing
        FakeApi api;
        g.render(api);
        EXPECT_EQ(api.map, first.map) << "tick " << i;
        EXPECT_FALSE(g.isOver()) << "tick " << i;
    }
    tick(g);
    EXPECT_TRUE(g.isOver());
    EXPECT_EQ(g.state(), SnakeState::Over);
}

TEST(Snake, SR_GAM_01_DrawsBoardWallsAndCells) {
    SnakeGame g(kTiles, kSeed);
    ASSERT_TRUE(g.setFood({0, 0}));
    FakeApi api;
    g.render(api);
    EXPECT_EQ(api.at(0, 50), kTiles.wall);
    EXPECT_EQ(api.at(127, 50), kTiles.wall);
    EXPECT_EQ(api.at(60, 2), kTiles.wall);
    EXPECT_EQ(api.at(60, 93), kTiles.wall);
    EXPECT_EQ(api.at(60, 1), 0);   // HUD row untouched
    EXPECT_EQ(api.at(60, 94), 0);  // hint row untouched
    // Food at cell (0, 0) covers columns 1-3, rows 3-5.
    EXPECT_EQ(api.at(1, 3), kTiles.food);
    EXPECT_EQ(api.at(3, 5), kTiles.food);
    EXPECT_EQ(api.at(4, 3), kTiles.empty);
    // Head at cell (21, 15): column 1 + 63 = 64, row 3 + 45 = 48.
    EXPECT_EQ(api.at(64, 48), kTiles.head);
    EXPECT_EQ(api.at(66, 50), kTiles.head);
    EXPECT_EQ(api.at(61, 48), kTiles.body);
    EXPECT_EQ(api.at(126, 92), kTiles.empty);
}

TEST(Snake, SR_GAM_17_EatSoundOnce) {
    SnakeGame g(kTiles, kSeed);
    ASSERT_TRUE(g.setFood({22, 15}));
    startAndMove(g);
    FakeApi api;
    g.render(api);
    g.render(api);  // e.g. paused: plays nothing
    tick(g);
    g.render(api);
    EXPECT_EQ(api.sounds, std::vector<EffectId>{EffectId::Eat});
}

TEST(Snake, SR_GAM_18_GameOverSoundOnce) {
    SnakeGame g(kTiles, kSeed);
    g.setFood({0, 0});
    g.update(keys({Action::Right}));
    FakeApi api;
    for (int i = 0; i < SNAKE_STEP_START * 21 + SNAKE_DEATH_TICKS; ++i) {
        tick(g);
        g.render(api);
    }
    EXPECT_TRUE(g.isOver());
    EXPECT_EQ(api.sounds, std::vector<EffectId>{EffectId::GameOver});
}

TEST(Snake, SR_FW_03_InitStartsANewRound) {
    SnakeGame g(kTiles, kSeed);
    ASSERT_TRUE(g.setFood({22, 15}));
    startAndMove(g);
    g.init();
    EXPECT_EQ(g.state(), SnakeState::Ready);
    EXPECT_EQ(g.length(), SNAKE_START_LEN);
    EXPECT_EQ(g.score(), 0U);
    EXPECT_EQ(g.stepTicks(), SNAKE_STEP_START);
}

}  // namespace
}  // namespace arcade
