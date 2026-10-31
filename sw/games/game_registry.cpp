#include "games/game_registry.hpp"

#include <memory>
#include <random>

#include "assets/game_tiles.hpp"
#include "games/breakout/breakout.hpp"
#include "games/snake/snake_game.hpp"

namespace arcade {
namespace {

// Games cannot see sw/assets/ (QA-01), so the factories pass the tile indices in. The seed is
// read once at start-up, never in a tick (R-4.1.6, SR-GAM-05).
std::unique_ptr<Game> makeSnake() {
    const SnakeTiles tiles{assets::kFontFirstTile, assets::kTileSnakeWall, assets::kTileSnakeBody,
                           assets::kTileSnakeHead, assets::kTileSnakeFood};
    std::random_device seed;
    return std::make_unique<SnakeGame>(tiles, seed());
}

std::unique_ptr<Game> makeBreakout() {
    return std::make_unique<Breakout>(assets::kTileBreakoutFirst, assets::kFontFirstTile);
}

}  // namespace

const std::array<GameRegistryRow, kGameRegistrySize> kGameRegistry{{
    {kGameIdSnake, "SNAKE", 0, &makeSnake},
    {kGameIdBreakout, "BREAKOUT", 1, &makeBreakout},
}};

}  // namespace arcade
