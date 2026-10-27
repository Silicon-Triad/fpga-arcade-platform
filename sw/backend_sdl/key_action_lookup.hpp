/// @file
/// Key-to-action lookup injected into the SDL2 backend. The backend holds no binding table:
/// TBL-KeyBinding and its lookup are issue #21 (SR-INP-09 to SR-INP-11).
#pragma once

#include <cstdint>
#include <optional>

#include "arcade/types.hpp"

namespace arcade {

/// Action a key maps to; `ch` is 'A'-'Z' when `action` is Action::Char, else 0 (SR-INP-11).
struct KeyMapping {
    Action action = Action::Up;
    char ch = 0;
};

/// Maps a Linux key code to an action in an input context (TBL-KeyBinding, SR-INP-09). Used
/// by the SDL2 backend so PC keys map to the same actions as on the board (SR-FW-06).
class KeyActionLookup {
  public:
    virtual ~KeyActionLookup() = default;

    /// Action bound to `keyCode` (`KEY_*`) in `context`, falling back to Global, or
    /// std::nullopt if the key is unbound there (SR-INP-09).
    virtual std::optional<KeyMapping> lookup(InputContext context,
                                             std::uint16_t keyCode) const noexcept = 0;
};

}  // namespace arcade
