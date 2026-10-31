#include "breakout.hpp"

#include <algorithm>

namespace arcade {
namespace {

constexpr int kFieldLeft = kBreakoutLeftWall + 1;    // first column inside the walls
constexpr int kFieldRight = kBreakoutRightWall - 1;  // last column inside the walls
constexpr int kFieldTop = kBreakoutTopWall + 1;      // first row below the top wall
// Last row the game draws on; rows 94-95 are the hint bar and frame (section 3.1.1).
constexpr int kFieldBottom = 93;

bool pressed(const InputFrame& input, Action action) noexcept {
    for (std::size_t i = 0; i < input.count; ++i) {
        if (input.events[i].type == InputEventType::KeyDown && input.events[i].action == action) {
            return true;
        }
    }
    return false;
}

bool held(const InputFrame& input, Action action) noexcept {
    return input.held[static_cast<std::size_t>(action)];
}

}  // namespace

Breakout::Breakout(TileIndex firstTile, TileIndex blankTile) noexcept
    : firstTile_(firstTile), blankTile_(blankTile) {
    init();
}

void Breakout::init() noexcept {
    fillWall();
    lives_ = START_LIVES;  // SR-GAM-13
    level_ = 1;
    score_ = 0;
    paddleX_ = kPaddleStartX;
    soundPending_ = false;
    serve();
}

// SR-GAM-07: a full wall of BRICK_COLS x BRICK_ROWS bricks.
void Breakout::fillWall() noexcept {
    for (auto& row : bricks_) {
        row.fill(true);
    }
    bricksLeft_ = BRICK_COLS * BRICK_ROWS;
}

// SR-GAM-08: the ball rests on the paddle, centred.
void Breakout::serve() noexcept {
    state_ = BreakoutState::Serve;
    ballX_ = paddleX_ + (PADDLE_W - BALL_SIZE) / 2;
    ballY_ = kPaddleRow - BALL_SIZE;
    ballDx_ = 1;
    ballDy_ = -1;
    ballTimer_ = 0;
}

bool Breakout::brick(int col, int row) const noexcept {
    return bricks_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
}

void Breakout::setBrickForTest(int col, int row, bool present) noexcept {
    if (brick(col, row) != present) {
        bricksLeft_ += present ? 1 : -1;
    }
    bricks_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = present;
}

void Breakout::setBallForTest(int x, int y, int dx, int dy) noexcept {
    ballX_ = x;
    ballY_ = y;
    ballDx_ = dx;
    ballDy_ = dy;
    ballTimer_ = 0;
    state_ = BreakoutState::Running;
}

void Breakout::queueSound(EffectId effect) noexcept {
    sound_ = effect;
    soundPending_ = true;
}

TileIndex Breakout::tile(BreakoutTile t) const noexcept {
    return static_cast<TileIndex>(firstTile_ + static_cast<TileIndex>(t));
}

void Breakout::update(const InputFrame& input) noexcept {
    if (state_ == BreakoutState::Over) {
        return;
    }
    if (state_ == BreakoutState::LevelClear) {
        // SR-GAM-14: pause, then a new full wall and the next level.
        if (++clearTimer_ >= kLevelClearTicks) {
            fillWall();
            ++level_;
            serve();
        }
        return;
    }

    // SR-GAM-09: one tile per tick while held, stopping at the side walls.
    if (held(input, Action::Left) && !held(input, Action::Right)) {
        paddleX_ = std::max(paddleX_ - 1, kPaddleMinX);
    } else if (held(input, Action::Right) && !held(input, Action::Left)) {
        paddleX_ = std::min(paddleX_ + 1, kPaddleMaxX);
    }

    if (state_ == BreakoutState::Serve) {
        serve();  // SR-GAM-08: the ball moves with the paddle
        if (pressed(input, Action::Launch)) {
            state_ = BreakoutState::Running;  // SR-GAM-08: moving up diagonally (dy = -1)
        }
        return;
    }

    // SR-GAM-10: one step every 2 ticks on level 1, every tick from level 2.
    const int period = level_ == 1 ? kBallPeriodLevel1 : kBallPeriodFast;
    if (++ballTimer_ >= period) {
        ballTimer_ = 0;
        stepBall();
    }
}

bool Breakout::hitsBrick(int x, int y) const noexcept {
    for (int row = 0; row < BRICK_ROWS; ++row) {
        const int top = kBrickTopRow + row * kBrickH;
        if (y + BALL_SIZE <= top || y >= top + kBrickH) {
            continue;
        }
        for (int col = 0; col < BRICK_COLS; ++col) {
            const int left = kFieldLeft + col * kBrickSlotW;
            if (brick(col, row) && x + BALL_SIZE > left && x < left + kBrickW) {
                return true;
            }
        }
    }
    return false;
}

// SR-GAM-12: remove every brick the ball overlaps and add its row's points.
void Breakout::removeBricks(int x, int y) noexcept {
    for (int row = 0; row < BRICK_ROWS; ++row) {
        const int top = kBrickTopRow + row * kBrickH;
        if (y + BALL_SIZE <= top || y >= top + kBrickH) {
            continue;
        }
        for (int col = 0; col < BRICK_COLS; ++col) {
            const int left = kFieldLeft + col * kBrickSlotW;
            if (brick(col, row) && x + BALL_SIZE > left && x < left + kBrickW) {
                bricks_[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = false;
                --bricksLeft_;
                score_ = std::min(score_ + ROW_POINTS[static_cast<std::size_t>(row)], SCORE_MAX);
            }
        }
    }
}

void Breakout::stepBall() noexcept {
    // SR-GAM-11: side walls and top wall.
    if (ballX_ + ballDx_ < kFieldLeft || ballX_ + ballDx_ + BALL_SIZE - 1 > kFieldRight) {
        ballDx_ = -ballDx_;
    }
    if (ballY_ + ballDy_ < kFieldTop) {
        ballDy_ = -ballDy_;
    }
    const int nx = ballX_ + ballDx_;
    const int ny = ballY_ + ballDy_;

    // SR-GAM-11, SR-GAM-12: bricks. Reverse the axis that hits; a corner-only hit reverses both.
    if (hitsBrick(nx, ny)) {
        bool flipY = hitsBrick(ballX_, ny);
        bool flipX = hitsBrick(nx, ballY_);
        if (!flipX && !flipY) {
            flipX = flipY = true;
        }
        removeBricks(nx, ny);
        if (flipX) {
            ballDx_ = -ballDx_;
        }
        if (flipY) {
            ballDy_ = -ballDy_;
        }
        queueSound(EffectId::BrickHit);  // SR-GAM-19
        if (bricksLeft_ == 0) {
            state_ = BreakoutState::LevelClear;  // SR-GAM-14
            clearTimer_ = 0;
        }
        return;
    }

    // SR-GAM-11: paddle; the half it hits sets the horizontal direction.
    if (ballDy_ > 0 && ny + BALL_SIZE - 1 == kPaddleRow && nx + BALL_SIZE > paddleX_ &&
        nx < paddleX_ + PADDLE_W) {
        ballDy_ = -1;
        // Compare centres in half tiles: ball centre 2*nx + BALL_SIZE, paddle 2*x + PADDLE_W.
        ballDx_ = (2 * nx + BALL_SIZE < 2 * paddleX_ + PADDLE_W) ? -1 : 1;
        queueSound(EffectId::PaddleHit);  // SR-GAM-20
        return;
    }

    ballX_ = nx;
    ballY_ = ny;

    // SR-GAM-13: the ball has passed below the paddle.
    if (ballY_ > kPaddleRow) {
        --lives_;
        if (lives_ > 0) {
            queueSound(EffectId::LifeLost);  // SR-GAM-21
            serve();
        } else {
            queueSound(EffectId::GameOver);  // SR-GAM-22
            state_ = BreakoutState::Over;
        }
    }
}

void Breakout::render(ArcadeApi& api) noexcept {
    // Section 3.1.1 screen 4: clear the play field, then walls, bricks, paddle and ball.
    api.fillRect(kFieldLeft, kFieldTop, kFieldRight - kFieldLeft + 1, kFieldBottom - kFieldTop + 1,
                 blankTile_);
    const TileIndex wall = tile(BreakoutTile::Wall);
    api.fillRect(kBreakoutLeftWall, kBreakoutTopWall, 1, kFieldBottom - kBreakoutTopWall + 1, wall);
    api.fillRect(kBreakoutRightWall, kBreakoutTopWall, 1, kFieldBottom - kBreakoutTopWall + 1,
                 wall);
    api.fillRect(kBreakoutLeftWall, kBreakoutTopWall, kGridCols, 1, wall);

    for (int row = 0; row < BRICK_ROWS; ++row) {
        const auto brickTile =
            static_cast<TileIndex>(tile(BreakoutTile::BrickRow0) + static_cast<TileIndex>(row));
        for (int col = 0; col < BRICK_COLS; ++col) {
            if (brick(col, row)) {
                api.fillRect(kFieldLeft + col * kBrickSlotW, kBrickTopRow + row * kBrickH, kBrickW,
                             kBrickH, brickTile);
            }
        }
    }

    api.fillRect(paddleX_, kPaddleRow, PADDLE_W, 1, tile(BreakoutTile::Paddle));

    if (state_ != BreakoutState::Over && ballY_ <= kFieldBottom - 1) {
        api.setTile(ballX_, ballY_, tile(BreakoutTile::BallTopLeft));
        api.setTile(ballX_ + 1, ballY_, tile(BreakoutTile::BallTopRight));
        api.setTile(ballX_, ballY_ + 1, tile(BreakoutTile::BallBottomLeft));
        api.setTile(ballX_ + 1, ballY_ + 1, tile(BreakoutTile::BallBottomRight));
    }

    // Game interface sound rule: each event's sound once, in the first render() after it.
    if (soundPending_) {
        api.playSound(sound_);
        soundPending_ = false;
    }
}

}  // namespace arcade
