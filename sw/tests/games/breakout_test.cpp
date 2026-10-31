// Unit tests for Breakout (issue #19, SR-GAM-07 to SR-GAM-14, SR-GAM-19 to SR-GAM-22).
#include "breakout.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace arcade {
namespace {

constexpr TileIndex kFirst = 200;  // any first tile; the engine passes the real one
constexpr TileIndex kBlank = 0;

TileIndex tileOf(BreakoutTile t) { return static_cast<TileIndex>(kFirst + static_cast<int>(t)); }

/// Records the tilemap and every playSound() call.
class FakeApi final : public ArcadeApi {
  public:
    FakeApi() { cells.fill(kBlank); }
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        if (col < 0 || col >= kGridCols || row < 0 || row >= kGridRows) {
            return ApiStatus::OutOfRange;
        }
        cells[index(col, row)] = tile;
        return ApiStatus::Ok;
    }
    ApiStatus fillRect(int col, int row, int w, int h, TileIndex tile) noexcept override {
        if (col < 0 || row < 0 || w < 0 || h < 0 || col + w > kGridCols || row + h > kGridRows) {
            return ApiStatus::OutOfRange;
        }
        for (int y = row; y < row + h; ++y) {
            for (int x = col; x < col + w; ++x) {
                setTile(x, y, tile);
            }
        }
        return ApiStatus::Ok;
    }
    ApiStatus drawText(int, int, std::string_view, TextScale) noexcept override {
        return ApiStatus::Ok;
    }
    void clear(TileIndex tile) noexcept override { cells.fill(tile); }
    ApiStatus present() noexcept override { return ApiStatus::Ok; }
    InputFrame pollInput() noexcept override { return {}; }
    void playSound(EffectId id) noexcept override { sounds.push_back(id); }
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return 0; }

    static std::size_t index(int col, int row) {
        return static_cast<std::size_t>(row) * kGridCols + static_cast<std::size_t>(col);
    }

    TileIndex at(int col, int row) const { return cells[index(col, row)]; }

    std::array<TileIndex, static_cast<std::size_t>(kGridCols) * kGridRows> cells{};
    std::vector<EffectId> sounds;
};

InputFrame holding(Action action) {
    InputFrame f;
    f.held[static_cast<std::size_t>(action)] = true;
    return f;
}

InputFrame pressing(Action action) {
    InputFrame f;
    f.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    f.count = 1;
    return f;
}

void tick(Breakout& game, int n = 1) {
    for (int i = 0; i < n; ++i) {
        game.update(InputFrame{});
    }
}

/// One ball step on level 1 (SR-GAM-10: every 2 ticks).
void step(Breakout& game) { tick(game, kBallPeriodLevel1); }

void clearAllBricks(Breakout& game) {
    for (int r = 0; r < BRICK_ROWS; ++r) {
        for (int c = 0; c < BRICK_COLS; ++c) {
            game.setBrickForTest(c, r, false);
        }
    }
}

/// Drops the ball past the paddle and runs until the life is lost.
void loseLife(Breakout& game) {
    game.setBallForTest(10, 88, 1, 1);  // paddle is far away at column 56
    for (int i = 0; i < 10 && game.state() == BreakoutState::Running; ++i) {
        step(game);
    }
}

// ---- SR-GAM-07: layout --------------------------------------------------------------------

TEST(Breakout, SR_GAM_07_InitialLayout) {
    Breakout game(kFirst, kBlank);
    FakeApi api;
    game.render(api);

    EXPECT_EQ(game.bricksLeft(), BRICK_COLS * BRICK_ROWS);
    for (int r = 0; r < BRICK_ROWS; ++r) {
        const auto brickTile = static_cast<TileIndex>(tileOf(BreakoutTile::BrickRow0) + r);
        for (int c = 0; c < BRICK_COLS; ++c) {
            const int left = 1 + c * 6;
            const int top = 8 + r * 2;
            for (int dx = 0; dx < 5; ++dx) {
                EXPECT_EQ(api.at(left + dx, top), brickTile);
                EXPECT_EQ(api.at(left + dx, top + 1), brickTile);
            }
            EXPECT_EQ(api.at(left + 5, top), kBlank) << "1-tile gap";
        }
    }
    EXPECT_EQ(api.at(1, 7), kBlank);
    EXPECT_EQ(api.at(1, 20), kBlank);
    // Paddle: 16 x 1 on row 90, columns 56-71 when centred (section 3.1.1 screen 4).
    for (int x = 56; x <= 71; ++x) {
        EXPECT_EQ(api.at(x, 90), tileOf(BreakoutTile::Paddle));
    }
    EXPECT_EQ(api.at(55, 90), kBlank);
    EXPECT_EQ(api.at(72, 90), kBlank);
    // Walls: column 0, column 127, row 2; the bottom is open.
    EXPECT_EQ(api.at(0, 50), tileOf(BreakoutTile::Wall));
    EXPECT_EQ(api.at(127, 50), tileOf(BreakoutTile::Wall));
    EXPECT_EQ(api.at(64, 2), tileOf(BreakoutTile::Wall));
    EXPECT_EQ(api.at(64, 93), kBlank);
    // Ball: 2 x 2 tiles resting on the paddle.
    EXPECT_EQ(api.at(63, 88), tileOf(BreakoutTile::BallTopLeft));
    EXPECT_EQ(api.at(64, 88), tileOf(BreakoutTile::BallTopRight));
    EXPECT_EQ(api.at(63, 89), tileOf(BreakoutTile::BallBottomLeft));
    EXPECT_EQ(api.at(64, 89), tileOf(BreakoutTile::BallBottomRight));
    EXPECT_TRUE(api.sounds.empty());
}

// ---- SR-GAM-08: serve and launch ----------------------------------------------------------

TEST(Breakout, SR_GAM_08_BallRestsOnPaddleAndMovesWithIt) {
    Breakout game(kFirst, kBlank);
    EXPECT_EQ(game.state(), BreakoutState::Serve);
    tick(game, 20);
    EXPECT_EQ(game.ballX(), 63);
    EXPECT_EQ(game.ballY(), 88);
    game.update(holding(Action::Right));
    EXPECT_EQ(game.paddleX(), 57);
    EXPECT_EQ(game.ballX(), 64);
    EXPECT_EQ(game.ballY(), 88);
}

TEST(Breakout, SR_GAM_08_LaunchMovesBallUpDiagonally) {
    Breakout game(kFirst, kBlank);
    game.update(pressing(Action::Launch));
    EXPECT_EQ(game.state(), BreakoutState::Running);
    step(game);
    EXPECT_EQ(game.ballY(), 87);
    EXPECT_EQ(game.ballX(), 64);
    step(game);
    EXPECT_EQ(game.ballY(), 86);
    EXPECT_EQ(game.ballX(), 65);
}

// ---- SR-GAM-09: paddle ---------------------------------------------------------------------

TEST(Breakout, SR_GAM_09_PaddleMovesOneTilePerTickAndStopsAtWalls) {
    Breakout game(kFirst, kBlank);
    game.update(holding(Action::Left));
    EXPECT_EQ(game.paddleX(), 55);
    for (int i = 0; i < 200; ++i) {
        game.update(holding(Action::Left));
    }
    EXPECT_EQ(game.paddleX(), 1);
    for (int i = 0; i < 200; ++i) {
        game.update(holding(Action::Right));
    }
    EXPECT_EQ(game.paddleX(), 111);  // App. A paddleX 1 ... 111
    tick(game);
    EXPECT_EQ(game.paddleX(), 111) << "no movement without a held key";
}

// ---- SR-GAM-10: ball speed -----------------------------------------------------------------

TEST(Breakout, SR_GAM_10_Level1MovesEveryTwoTicks) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(60, 50, 1, -1);
    tick(game);
    EXPECT_EQ(game.ballY(), 50);
    tick(game);
    EXPECT_EQ(game.ballY(), 49);
    tick(game);
    EXPECT_EQ(game.ballY(), 49);
    tick(game);
    EXPECT_EQ(game.ballY(), 48);
}

// ---- SR-GAM-11: bounces --------------------------------------------------------------------

TEST(Breakout, SR_GAM_11_BouncesOffLeftWall) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(1, 50, -1, -1);
    step(game);
    EXPECT_EQ(game.ballDx(), 1);
    EXPECT_EQ(game.ballX(), 2);
    EXPECT_EQ(game.ballY(), 49);
}

TEST(Breakout, SR_GAM_11_BouncesOffRightWall) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(125, 50, 1, -1);  // occupies columns 125-126
    step(game);
    EXPECT_EQ(game.ballDx(), -1);
    EXPECT_EQ(game.ballX(), 124);
}

TEST(Breakout, SR_GAM_11_BouncesOffTopWall) {
    Breakout game(kFirst, kBlank);
    clearAllBricks(game);
    game.setBallForTest(60, 3, 1, -1);  // row 3 is just below the wall on row 2
    step(game);
    EXPECT_EQ(game.ballDy(), 1);
    EXPECT_EQ(game.ballY(), 4);
}

TEST(Breakout, SR_GAM_11_BouncesOffBrickBelow) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(2, 20, 1, -1);  // just under bottom-row brick 0 (rows 18-19)
    step(game);
    EXPECT_FALSE(game.brick(0, 5));
    EXPECT_EQ(game.ballDy(), 1);
    EXPECT_EQ(game.ballDx(), 1);
}

TEST(Breakout, SR_GAM_11_BouncesOffBrickSide) {
    Breakout game(kFirst, kBlank);
    clearAllBricks(game);
    game.setBrickForTest(1, 5, true);  // columns 7-11, rows 18-19
    game.setBallForTest(5, 17, 1, 1);
    step(game);
    EXPECT_FALSE(game.brick(1, 5));
    EXPECT_EQ(game.ballDx(), -1);
    EXPECT_EQ(game.ballDy(), 1);
}

TEST(Breakout, SR_GAM_11_BouncesOffBrickCorner) {
    Breakout game(kFirst, kBlank);
    clearAllBricks(game);
    game.setBrickForTest(1, 5, true);
    game.setBallForTest(5, 20, 1, -1);  // only the diagonal cell touches the brick
    step(game);
    EXPECT_FALSE(game.brick(1, 5));
    EXPECT_EQ(game.ballDx(), -1);
    EXPECT_EQ(game.ballDy(), 1);
}

TEST(Breakout, SR_GAM_11_LeftHalfOfPaddleSendsBallLeft) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(56, 88, 1, 1);  // lands on columns 57-58 of paddle 56-71
    step(game);
    EXPECT_EQ(game.ballDy(), -1);
    EXPECT_EQ(game.ballDx(), -1);
    EXPECT_EQ(game.ballY(), 88);
}

TEST(Breakout, SR_GAM_11_RightHalfOfPaddleSendsBallRight) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(66, 88, -1, 1);  // lands on columns 65-66
    step(game);
    EXPECT_EQ(game.ballDy(), -1);
    EXPECT_EQ(game.ballDx(), 1);
}

TEST(Breakout, SR_GAM_11_PaddleEdgeHitsAndMissBeyondIt) {
    Breakout edge(kFirst, kBlank);
    edge.setBallForTest(54, 88, 1, 1);  // lands on columns 55-56: one tile on the paddle
    step(edge);
    EXPECT_EQ(edge.ballDy(), -1);
    EXPECT_EQ(edge.ballDx(), -1);

    Breakout miss(kFirst, kBlank);
    miss.setBallForTest(53, 88, 1, 1);  // lands on columns 54-55: off the paddle
    step(miss);
    EXPECT_EQ(miss.ballDy(), 1);
    EXPECT_EQ(miss.ballY(), 89);
}

// ---- SR-GAM-12: points ---------------------------------------------------------------------

TEST(Breakout, SR_GAM_12_RowPointsSixtyDownToTen) {
    const std::array<std::uint32_t, BRICK_ROWS> expected{60, 50, 40, 30, 20, 10};
    for (int r = 0; r < BRICK_ROWS; ++r) {
        Breakout game(kFirst, kBlank);
        clearAllBricks(game);
        game.setBrickForTest(0, r, true);
        game.setBallForTest(2, 8 + r * 2 + 2, 1, -1);  // just below the brick
        step(game);
        EXPECT_FALSE(game.brick(0, r));
        EXPECT_EQ(game.score(), expected[static_cast<std::size_t>(r)]) << "row " << r;
    }
}

// ---- SR-GAM-13: lives ----------------------------------------------------------------------

TEST(Breakout, SR_GAM_13_StartsWithThreeLives) {
    Breakout game(kFirst, kBlank);
    EXPECT_EQ(game.lives(), 3);
    EXPECT_FALSE(game.isOver());
}

TEST(Breakout, SR_GAM_13_BallBelowPaddleLosesLifeAndServes) {
    Breakout game(kFirst, kBlank);
    loseLife(game);
    EXPECT_EQ(game.lives(), 2);
    EXPECT_EQ(game.state(), BreakoutState::Serve);
    EXPECT_EQ(game.ballY(), 88);
}

TEST(Breakout, SR_GAM_13_NoLivesLeftEndsGame) {
    Breakout game(kFirst, kBlank);
    loseLife(game);
    loseLife(game);
    EXPECT_FALSE(game.isOver());
    loseLife(game);
    EXPECT_EQ(game.lives(), 0);
    EXPECT_TRUE(game.isOver());
    game.update(pressing(Action::Launch));
    EXPECT_TRUE(game.isOver());
}

TEST(Breakout, SR_GAM_13_InitStartsNewGame) {
    Breakout game(kFirst, kBlank);
    loseLife(game);
    game.setBrickForTest(0, 0, false);
    game.init();
    EXPECT_EQ(game.lives(), 3);
    EXPECT_EQ(game.score(), 0U);
    EXPECT_EQ(game.level(), 1);
    EXPECT_EQ(game.bricksLeft(), BRICK_COLS * BRICK_ROWS);
}

// ---- SR-GAM-14: level clear ----------------------------------------------------------------

TEST(Breakout, SR_GAM_14_LevelClearPauses60TicksThenNewWallAndFasterBall) {
    Breakout game(kFirst, kBlank);
    clearAllBricks(game);
    game.setBrickForTest(0, 5, true);
    game.setBallForTest(2, 20, 1, -1);
    step(game);
    ASSERT_EQ(game.state(), BreakoutState::LevelClear);
    tick(game, kLevelClearTicks - 1);
    EXPECT_EQ(game.state(), BreakoutState::LevelClear);
    EXPECT_EQ(game.bricksLeft(), 0);
    EXPECT_EQ(game.level(), 1);
    tick(game);
    EXPECT_EQ(game.state(), BreakoutState::Serve);
    EXPECT_EQ(game.bricksLeft(), BRICK_COLS * BRICK_ROWS);
    EXPECT_EQ(game.level(), 2);

    // SR-GAM-10: from level 2 the ball moves every tick.
    game.update(pressing(Action::Launch));
    const int y = game.ballY();
    tick(game);
    EXPECT_EQ(game.ballY(), y - 1);
    tick(game);
    EXPECT_EQ(game.ballY(), y - 2);
}

// ---- SR-GAM-19 to SR-GAM-22: sounds, each exactly once -----------------------------------

void expectSoundOnce(Breakout& game, EffectId effect) {
    FakeApi api;
    game.render(api);
    game.render(api);  // e.g. while paused: plays nothing
    ASSERT_EQ(api.sounds.size(), 1U);
    EXPECT_EQ(api.sounds[0], effect);
}

TEST(Breakout, SR_GAM_19_BrickHitPlaysOnce) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(2, 20, 1, -1);
    step(game);
    expectSoundOnce(game, EffectId::BrickHit);
}

TEST(Breakout, SR_GAM_20_PaddleHitPlaysOnce) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(60, 88, 1, 1);
    step(game);
    expectSoundOnce(game, EffectId::PaddleHit);
}

TEST(Breakout, SR_GAM_21_LifeLostPlaysOnce) {
    Breakout game(kFirst, kBlank);
    loseLife(game);
    expectSoundOnce(game, EffectId::LifeLost);
}

TEST(Breakout, SR_GAM_22_LastLifeLostPlaysGameOverOnce) {
    Breakout game(kFirst, kBlank);
    FakeApi api;
    loseLife(game);
    game.render(api);
    loseLife(game);
    game.render(api);
    api.sounds.clear();
    loseLife(game);
    game.render(api);
    game.render(api);
    ASSERT_EQ(api.sounds.size(), 1U);
    EXPECT_EQ(api.sounds[0], EffectId::GameOver);
}

TEST(Breakout, NoSoundWithoutEvent) {
    Breakout game(kFirst, kBlank);
    game.setBallForTest(60, 50, 1, -1);
    step(game);
    FakeApi api;
    game.render(api);
    EXPECT_TRUE(api.sounds.empty());
}

}  // namespace
}  // namespace arcade
