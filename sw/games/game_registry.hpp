/// @file
/// TBL-GameRegistry contents. Adding a game needs one class and one row here, with no change to
/// the Game Engine, the HAL or the FPGA design (SR-FW-04, QA-02).
#pragma once

#include <array>

#include "arcade/tables.hpp"

namespace arcade {

/// Number of registered games (TBL-GameRegistry rows).
constexpr std::size_t kGameRegistrySize = 2;

/// The registry rows (0, "SNAKE", 0) and (1, "BREAKOUT", 1) (TBL-GameRegistry, SR-FW-03).
extern const std::array<GameRegistryRow, kGameRegistrySize> kGameRegistry;

}  // namespace arcade
