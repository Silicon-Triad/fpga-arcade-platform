/// @file
/// Snake (SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18, SF-01).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

#include "arcade/arcade_api.hpp"
#include "arcade/game.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Board width in Snake cells (SNAKE_BOARD, SR-GAM-01).
constexpr int kSnakeBoardCols = 42;
/// Board height in Snake cells (SNAKE_BOARD, SR-GAM-01).
constexpr int kSnakeBoardRows = 30;
/// Number of board cells; the longest possible snake (App. A snakeBody, 1,260 = 42 x 30).
constexpr std::size_t kSnakeBoardCells =
    static_cast<std::size_t>(kSnakeBoardCols) * static_cast<std::size_t>(kSnakeBoardRows);
/// Width and height of one Snake cell in tiles (SNAKE_CELL, SR-GAM-01).
constexpr int kSnakeCellTiles = 3;
/// Tile column of board cell x = 0 (SR-GAM-01: columns 1-126).
constexpr int kSnakeBoardCol0 = 1;
/// Tile row of board cell y = 0 (SR-GAM-01: rows 3-92).
constexpr int kSnakeBoardRow0 = 3;
/// Initial snake length in cells (SNAKE_START_LEN, SR-GAM-02).
constexpr std::size_t SNAKE_START_LEN = 4;
/// Ticks per move at the start (SNAKE_STEP_START, SR-GAM-03).
constexpr int SNAKE_STEP_START = 6;
/// Fewest ticks per move (SNAKE_STEP_MIN, SR-GAM-03).
constexpr int SNAKE_STEP_MIN = 3;
/// Food items eaten between speed-ups (SR-GAM-03).
constexpr std::uint32_t kSnakeFoodPerSpeedUp = 5;
/// Points per food item (FOOD_POINTS, SR-GAM-05).
constexpr std::uint32_t FOOD_POINTS = 10;
/// Ticks the final frame is held after death (SNAKE_DEATH_TICKS, SR-GAM-06, SF-01).
constexpr int SNAKE_DEATH_TICKS = 30;

/// Tile indices Snake draws with. The Game Engine passes them in, because games may include
/// only arcade/*.hpp (QA-01, QA-03) and the tile numbering lives in sw/assets/.
struct SnakeTiles {
    TileIndex empty = 0;  ///< Empty board cell.
    TileIndex wall = 0;   ///< Border wall (SR-GAM-01).
    TileIndex body = 0;   ///< Snake body, dark green (section 3.1.1 screen 3).
    TileIndex head = 0;   ///< Snake head, bright green (section 3.1.1 screen 3).
    TileIndex food = 0;   ///< Food, red (section 3.1.1 screen 3).
};

/// A Snake board cell, x 0-41 and y 0-29 (App. A foodPos).
struct SnakeCell {
    int x = 0;
    int y = 0;
    friend bool operator==(SnakeCell a, SnakeCell b) noexcept { return a.x == b.x && a.y == b.y; }
};

/// Snake direction (App. A snakeDir / nextDir).
enum class SnakeDir : std::uint8_t { Up, Down, Left, Right };

/// Snake's internal states below the application's Playing state (srs-muhammad.md 3.3).
enum class SnakeState : std::uint8_t { Ready, Running, Dead, Over };

/// The Snake game (SR-GAM-01 to SR-GAM-06, SR-GAM-17, SR-GAM-18). A pure state machine
/// driven by update(); no heap allocation, all storage is fixed-size (R-4.1.6).
class SnakeGame final : public Game {
  public:
    /// Creates a game that draws with `tiles` and places food from a generator seeded with
    /// `seed`, so tests are repeatable (SR-GAM-05). Calls init().
    SnakeGame(const SnakeTiles& tiles, std::uint32_t seed) noexcept;

    /// Starts a new round: Ready, length 4 in the centre heading right (SR-GAM-02, SR-GAM-03).
    void init() noexcept override;
    /// Applies arrow keys and moves the snake every stepTicks ticks (SR-GAM-02 to SR-GAM-06).
    void update(const InputFrame& input) noexcept override;
    /// Draws walls, board, snake and food, and plays the last update's sounds once
    /// (SR-GAM-01, SR-GAM-17, SR-GAM-18).
    void render(ArcadeApi& api) noexcept override;
    /// True once the 30-tick death frame has been held (SR-GAM-06).
    bool isOver() const noexcept override { return state_ == SnakeState::Over; }
    /// Current score (SR-GAM-05).
    std::uint32_t score() const noexcept override { return score_; }

    /// Current internal state (srs-muhammad.md 3.3).
    SnakeState state() const noexcept { return state_; }
    /// Snake length in cells.
    std::size_t length() const noexcept { return length_; }
    /// Cell `i` of the snake, head first (App. A snakeBody); `i` < length().
    SnakeCell cell(std::size_t i) const noexcept;
    /// Direction of the last move (App. A snakeDir).
    SnakeDir direction() const noexcept { return dir_; }
    /// Ticks per move (App. A stepTicks, SR-GAM-03).
    int stepTicks() const noexcept { return stepTicks_; }
    /// Current food cell (App. A foodPos).
    SnakeCell food() const noexcept { return food_; }
    /// Moves the food to `c` for tests and demos. @return false, and nothing changed, if `c`
    /// is off the board or under the snake (SR-GAM-05).
    bool setFood(SnakeCell c) noexcept;

  private:
    static std::size_t indexOf(SnakeCell c) noexcept;
    void turn(SnakeDir d) noexcept;
    void step() noexcept;
    void die() noexcept;
    bool placeFood() noexcept;

    SnakeTiles tiles_;
    std::mt19937 rng_;
    SnakeState state_ = SnakeState::Ready;
    // Ring buffer of the body, head at body_[head_], tail at body_[head_ + length_ - 1].
    std::array<SnakeCell, kSnakeBoardCells> body_{};
    std::array<bool, kSnakeBoardCells> occupied_{};
    std::size_t head_ = 0;
    std::size_t length_ = 0;
    SnakeDir dir_ = SnakeDir::Right;
    SnakeDir nextDir_ = SnakeDir::Right;
    bool turnTaken_ = false;
    int stepTicks_ = SNAKE_STEP_START;
    int tickCount_ = 0;
    int deadTicks_ = 0;
    std::uint32_t foodEaten_ = 0;
    std::uint32_t score_ = 0;
    SnakeCell food_{};
    bool playEat_ = false;
    bool playGameOver_ = false;
};

}  // namespace arcade
