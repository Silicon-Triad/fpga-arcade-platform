// VIOLATIONS (QA-01): a game may include only arcade/*.hpp and the standard library.
#include "hal/register_map.hpp"   // bypasses the Arcade API and the HAL
#include "engine/engine.hpp"      // calls the layer above it
#include <SDL2/SDL.h>             // not the standard library
#include "../other_game/other.hpp"  // another game's code
// Allowed lines, which must not be reported:
#include "arcade/arcade_api.hpp"
#include <vector>
// #include "hal/register_map.hpp"
/* #include "hal/register_map.hpp" */
#include "arcade/../engine/engine.hpp"  // escapes arcade/ with ..
#include <stdint.h>                     // C form; use <cstdint>
