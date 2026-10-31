/// @file
/// Breakout (SR-GAM-07 to SR-GAM-14, SR-GAM-19 to SR-GAM-22).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "arcade/arcade_api.hpp"
#include "arcade/game.hpp"
#include "arcade/tables.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Bricks per row (BRICK_COLS, SR-GAM-07).
constexpr int BRICK_COLS = 21;
/// Brick rows (BRICK_ROWS, SR-GAM-07).
constexpr int BRICK_ROWS = 6;
/// Paddle width in tiles (PADDLE_W, SR-GAM-07).
constexpr int PADDLE_W = 16;
/// Ball width and height in tiles (BALL_SIZE, SR-GAM-07).
constexpr int BALL_SIZE = 2;
/// Lives at the start of a game (START_LIVES, SR-GAM-13).
constexpr int START_LIVES = 3;
/// Brick points, top row to bottom row (ROW_POINTS, SR-GAM-12).
constexpr std::array<std::uint32_t, BRICK_ROWS> ROW_POINTS{60, 50, 40, 30, 20, 10};

/// Left wall column (section 3.1.1 screen 4).
constexpr int kBreakoutLeftWall = 0;
/// Right wall column (section 3.1.1 screen 4).
constexpr int kBreakoutRightWall = kGridCols - 1;
/// Top wall row (section 3.1.1 screen 4).
constexpr int kBreakoutTopWall = 2;
/// First tile row of the wall of bricks (SR-GAM-07: rows 8-19).
constexpr int kBrickTopRow = 8;
/// Brick width in tiles (SR-GAM-07).
constexpr int kBrickW = 5;
/// Brick height in tiles (SR-GAM-07).
constexpr int kBrickH = 2;
/// Brick slot width in tiles: brick plus a 1-tile gap (SR-GAM-07).
constexpr int kBrickSlotW = 6;
/// Paddle tile row (SR-GAM-07).
constexpr int kPaddleRow = 90;
/// Paddle left edge when centred (section 3.1.1 screen 4: columns 56-71).
constexpr int kPaddleStartX = 56;
/// Lowest paddle left edge (App. A paddleX 1 ... 111, SR-GAM-09).
constexpr int kPaddleMinX = kBreakoutLeftWall + 1;
/// Highest paddle left edge (App. A paddleX 1 ... 111, SR-GAM-09).
constexpr int kPaddleMaxX = kBreakoutRightWall - PADDLE_W;
/// Ticks per ball step on level 1 (SR-GAM-10).
constexpr int kBallPeriodLevel1 = 2;
/// Ticks per ball step from level 2 (SR-GAM-10).
constexpr int kBallPeriodFast = 1;
/// Ticks the game pauses after the last brick is removed (SR-GAM-14).
constexpr int kLevelClearTicks = 60;

/// Breakout's graphics, as offsets from the first tile passed to the constructor.
enum class BreakoutTile : std::uint8_t {
    Wall,
    BrickRow0,  ///< Top row, red; rows 1-5 follow (section 3.1.1 screen 4).
    BrickRow1,
    BrickRow2,
    BrickRow3,
    BrickRow4,
    BrickRow5,
    Paddle,
    BallTopLeft,  ///< The ball is drawn round with 4 tiles (section 3.1.1 screen 4).
    BallTopRight,
    BallBottomLeft,
    BallBottomRight,
};
/// Number of Breakout tiles.
constexpr std::size_t kBreakoutTileCount = 12;

/// Breakout state machine (App. A breakoutState).
enum class BreakoutState : std::uint8_t { Serve, Running, LevelClear, Over };

/// Breakout, a pure state machine driven by update() (SR-FW-03, QA-04).
class Breakout final : public Game {
  public:
    /// `firstTile` is the tile index of BreakoutTile::Wall; `blankTile` fills empty cells.
    /// Both come from the tile set the engine loads (SR-FW-01), which games cannot include
    /// (QA-01).
    Breakout(TileIndex firstTile, TileIndex blankTile) noexcept;

    /// Starts a new game: full wall, level 1, START_LIVES lives, Serve (SR-GAM-07, SR-GAM-13).
    void init() noexcept override;
    /// One tick: paddle, launch, ball, level clear (SR-GAM-08 to SR-GAM-14).
    void update(const InputFrame& input) noexcept override;
    /// Draws walls, bricks, paddle and ball, and plays this tick's sounds once
    /// (SR-GAM-07, SR-GAM-19 to SR-GAM-22).
    void render(ArcadeApi& api) noexcept override;
    /// True once the last life is lost (SR-GAM-13).
    bool isOver() const noexcept override { return state_ == BreakoutState::Over; }
    /// Points so far, at most SCORE_MAX (SR-GAM-12).
    std::uint32_t score() const noexcept override { return score_; }

    BreakoutState state() const noexcept { return state_; }
    int paddleX() const noexcept { return paddleX_; }
    int ballX() const noexcept { return ballX_; }
    int ballY() const noexcept { return ballY_; }
    int ballDx() const noexcept { return ballDx_; }
    int ballDy() const noexcept { return ballDy_; }
    int lives() const noexcept { return lives_; }
    int level() const noexcept { return level_; }
    bool brick(int col, int row) const noexcept;
    int bricksLeft() const noexcept { return bricksLeft_; }

    /// Test hook: places the ball and its direction and sets Running.
    void setBallForTest(int x, int y, int dx, int dy) noexcept;
    /// Test hook: sets the paddle's left edge.
    void setPaddleForTest(int x) noexcept { paddleX_ = x; }
    /// Test hook: sets one brick.
    void setBrickForTest(int col, int row, bool present) noexcept;

  private:
    void fillWall() noexcept;
    void serve() noexcept;
    void stepBall() noexcept;
    bool hitsBrick(int x, int y) const noexcept;
    void removeBricks(int x, int y) noexcept;
    void queueSound(EffectId effect) noexcept;
    TileIndex tile(BreakoutTile t) const noexcept;

    TileIndex firstTile_;
    TileIndex blankTile_;
    BreakoutState state_ = BreakoutState::Serve;
    std::array<std::array<bool, BRICK_COLS>, BRICK_ROWS> bricks_{};
    int bricksLeft_ = 0;
    int paddleX_ = kPaddleStartX;
    int ballX_ = 0;
    int ballY_ = 0;
    int ballDx_ = 1;
    int ballDy_ = -1;
    int ballTimer_ = 0;
    int clearTimer_ = 0;
    int lives_ = START_LIVES;
    int level_ = 1;
    std::uint32_t score_ = 0;
    bool soundPending_ = false;
    EffectId sound_ = EffectId::BrickHit;
};

}  // namespace arcade
