// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// The game interface every game implements; the Game Engine calls games only through it
/// (SR-FW-03).
#pragma once

#include <cstdint>

#include "arcade/arcade_api.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Abstract game (SR-FW-03). A game is a pure state machine driven by update(); it draws only
/// through the Arcade API passed to render() (SR-FW-05, QA-04). No call throws (R-4.1.5).
class Game {
  public:
    virtual ~Game() = default;

    /// Resets the game to its starting state for a new round (SR-FW-03).
    virtual void init() noexcept = 0;

    /// Advances the game by one tick using this tick's input (SR-FW-02, SR-FW-03).
    virtual void update(const InputFrame& input) noexcept = 0;

    /// Draws the current state through `api`; does not call present() (SR-FW-02, SR-FW-03).
    virtual void render(ArcadeApi& api) noexcept = 0;

    /// True once the game has ended (SR-FW-03).
    virtual bool isOver() const noexcept = 0;

    /// Current score, 0 to SCORE_MAX (SR-FW-03, TBL-HighScore score UINT32).
    virtual std::uint32_t score() const noexcept = 0;
};

}  // namespace arcade
