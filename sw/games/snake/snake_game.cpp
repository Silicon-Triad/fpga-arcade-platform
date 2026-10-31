// Snake (issue #18): SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18, SF-01.
#include "snake_game.hpp"

#include <algorithm>

#include "arcade/tables.hpp"

namespace arcade {
namespace {

// SR-GAM-01: walls on the border of the board, at column 0, column 127, row 2 and row 93
// (section 3.1.1 screen 3).
constexpr int kWallLeftCol = kSnakeBoardCol0 - 1;
constexpr int kWallTopRow = kSnakeBoardRow0 - 1;
constexpr int kBoardWidthTiles = kSnakeBoardCols * kSnakeCellTiles;
constexpr int kBoardHeightTiles = kSnakeBoardRows * kSnakeCellTiles;
constexpr int kWallRightCol = kSnakeBoardCol0 + kBoardWidthTiles;
constexpr int kWallBottomRow = kSnakeBoardRow0 + kBoardHeightTiles;
static_assert(kWallRightCol == kGridCols - 1, "SR-GAM-01: board ends at column 126");
static_assert(kWallBottomRow == 93, "SR-GAM-01: board ends at row 92");

bool isReverse(SnakeDir a, SnakeDir b) noexcept {
    switch (a) {
        case SnakeDir::Up:
            return b == SnakeDir::Down;
        case SnakeDir::Down:
            return b == SnakeDir::Up;
        case SnakeDir::Left:
            return b == SnakeDir::Right;
        case SnakeDir::Right:
            return b == SnakeDir::Left;
    }
    return false;
}

bool toDir(Action a, SnakeDir& out) noexcept {
    switch (a) {
        case Action::Up:
            out = SnakeDir::Up;
            return true;
        case Action::Down:
            out = SnakeDir::Down;
            return true;
        case Action::Left:
            out = SnakeDir::Left;
            return true;
        case Action::Right:
            out = SnakeDir::Right;
            return true;
        default:
            return false;
    }
}

void drawCell(ArcadeApi& api, SnakeCell c, TileIndex tile) noexcept {
    // SR-GAM-01: each cell is 3 x 3 tiles.
    api.fillRect(kSnakeBoardCol0 + c.x * kSnakeCellTiles, kSnakeBoardRow0 + c.y * kSnakeCellTiles,
                 kSnakeCellTiles, kSnakeCellTiles, tile);
}

}  // namespace

SnakeGame::SnakeGame(const SnakeTiles& tiles, std::uint32_t seed) noexcept
    : tiles_(tiles), rng_(seed) {
    init();
}

void SnakeGame::init() noexcept {
    // SR-GAM-02: Ready, length 4, centre of the board, heading right.
    state_ = SnakeState::Ready;
    occupied_.fill(false);
    head_ = 0;
    length_ = SNAKE_START_LEN;
    const SnakeCell centre{kSnakeBoardCols / 2, kSnakeBoardRows / 2};
    for (std::size_t i = 0; i < length_; ++i) {
        body_[i] = SnakeCell{centre.x - static_cast<int>(i), centre.y};
        occupied_[indexOf(body_[i])] = true;
    }
    dir_ = SnakeDir::Right;
    nextDir_ = SnakeDir::Right;
    turnTaken_ = false;
    stepTicks_ = SNAKE_STEP_START;  // SR-GAM-03
    tickCount_ = 0;
    deadTicks_ = 0;
    foodEaten_ = 0;
    score_ = 0;
    playEat_ = false;
    playGameOver_ = false;
    placeFood();  // SR-GAM-05
}

SnakeCell SnakeGame::cell(std::size_t i) const noexcept {
    return body_[(head_ + i) % kSnakeBoardCells];
}

std::size_t SnakeGame::indexOf(SnakeCell c) noexcept {
    return static_cast<std::size_t>(c.y) * static_cast<std::size_t>(kSnakeBoardCols) +
           static_cast<std::size_t>(c.x);
}

bool SnakeGame::setFood(SnakeCell c) noexcept {
    if (c.x < 0 || c.x >= kSnakeBoardCols || c.y < 0 || c.y >= kSnakeBoardRows ||
        occupied_[indexOf(c)]) {
        return false;
    }
    food_ = c;
    return true;
}

bool SnakeGame::placeFood() noexcept {
    // SR-GAM-05: a random cell not occupied by the snake, uniform over the free cells.
    const std::size_t freeCells = kSnakeBoardCells - length_;
    if (freeCells == 0) {
        return false;
    }
    std::uniform_int_distribution<std::size_t> pick(0, freeCells - 1);
    std::size_t k = pick(rng_);
    for (std::size_t i = 0; i < kSnakeBoardCells; ++i) {
        if (occupied_[i]) {
            continue;
        }
        if (k == 0) {
            food_ = SnakeCell{static_cast<int>(i % kSnakeBoardCols),
                              static_cast<int>(i / kSnakeBoardCols)};
            return true;
        }
        --k;
    }
    return false;
}

void SnakeGame::turn(SnakeDir d) noexcept {
    // SR-GAM-04: a reverse key is ignored, and at most one change applies per move.
    if (turnTaken_ || d == dir_ || isReverse(dir_, d)) {
        return;
    }
    nextDir_ = d;
    turnTaken_ = true;
}

void SnakeGame::update(const InputFrame& input) noexcept {
    // game.hpp sound rule: only this update's events are played by the next render().
    playEat_ = false;
    playGameOver_ = false;

    if (state_ == SnakeState::Over) {
        return;
    }
    if (state_ == SnakeState::Dead) {
        // SR-GAM-06, SF-01: hold the final frame unchanged for 30 ticks, then end.
        if (++deadTicks_ >= SNAKE_DEATH_TICKS) {
            state_ = SnakeState::Over;
        }
        return;
    }

    const std::size_t count = std::min(input.count, input.events.size());
    for (std::size_t i = 0; i < count; ++i) {
        const InputEvent& e = input.events[i];
        SnakeDir d{};
        if (e.type != InputEventType::KeyDown || !toDir(e.action, d)) {
            continue;
        }
        state_ = SnakeState::Running;  // SR-GAM-02: the first arrow key starts movement
        turn(d);
    }
    if (state_ != SnakeState::Running) {
        return;
    }

    // SR-GAM-03: one move every stepTicks ticks.
    if (++tickCount_ >= stepTicks_) {
        tickCount_ = 0;
        step();
    }
}

void SnakeGame::step() noexcept {
    dir_ = nextDir_;
    turnTaken_ = false;
    SnakeCell next = cell(0);
    switch (dir_) {
        case SnakeDir::Up:
            --next.y;
            break;
        case SnakeDir::Down:
            ++next.y;
            break;
        case SnakeDir::Left:
            --next.x;
            break;
        case SnakeDir::Right:
            ++next.x;
            break;
    }

    // SR-GAM-06: moving into a wall ends the game.
    if (next.x < 0 || next.x >= kSnakeBoardCols || next.y < 0 || next.y >= kSnakeBoardRows) {
        die();
        return;
    }
    const bool eating = next == food_;
    if (!eating) {
        // The tail leaves its cell in this move, so the head may enter it.
        occupied_[indexOf(cell(length_ - 1))] = false;
        --length_;
    }
    // SR-GAM-06: moving into the body ends the game.
    if (occupied_[indexOf(next)]) {
        ++length_;  // not eating here: the food is never under the snake
        occupied_[indexOf(cell(length_ - 1))] = true;  // keep the final frame unchanged
        die();
        return;
    }
    head_ = (head_ + kSnakeBoardCells - 1) % kSnakeBoardCells;
    body_[head_] = next;
    occupied_[indexOf(next)] = true;
    ++length_;

    if (eating) {
        // SR-GAM-05: grow by one cell (the tail stayed) and add 10 points.
        score_ = std::min(score_ + FOOD_POINTS, SCORE_MAX);
        ++foodEaten_;
        // SR-GAM-03: 1 tick faster after every 5 food items, down to 3.
        if (foodEaten_ % kSnakeFoodPerSpeedUp == 0) {
            stepTicks_ = std::max(stepTicks_ - 1, SNAKE_STEP_MIN);
        }
        playEat_ = true;  // SR-GAM-17
        if (!placeFood()) {
            die();  // board full: no cell left for food, so play cannot go on
        }
    }
}

void SnakeGame::die() noexcept {
    state_ = SnakeState::Dead;  // SR-GAM-06
    deadTicks_ = 0;
    playGameOver_ = true;  // SR-GAM-18
}

void SnakeGame::render(ArcadeApi& api) noexcept {
    // SR-GAM-01: walls and the 42 x 30 board on columns 1-126, rows 3-92. The Dead state draws
    // the same frame every tick, so nothing flashes (SR-GAM-06, SF-01).
    api.fillRect(kWallLeftCol, kWallTopRow, kGridCols, 1, tiles_.wall);
    api.fillRect(kWallLeftCol, kWallBottomRow, kGridCols, 1, tiles_.wall);
    api.fillRect(kWallLeftCol, kSnakeBoardRow0, 1, kBoardHeightTiles, tiles_.wall);
    api.fillRect(kWallRightCol, kSnakeBoardRow0, 1, kBoardHeightTiles, tiles_.wall);
    api.fillRect(kSnakeBoardCol0, kSnakeBoardRow0, kBoardWidthTiles, kBoardHeightTiles,
                 tiles_.empty);
    if (!occupied_[indexOf(food_)]) {  // only false once the board is full
        drawCell(api, food_, tiles_.food);
    }
    for (std::size_t i = length_; i-- > 1;) {
        drawCell(api, cell(i), tiles_.body);
    }
    drawCell(api, cell(0), tiles_.head);

    if (playEat_) {
        api.playSound(EffectId::Eat);  // SR-GAM-17
        playEat_ = false;
    }
    if (playGameOver_) {
        api.playSound(EffectId::GameOver);  // SR-GAM-18
        playGameOver_ = false;
    }
}

}  // namespace arcade
