/// @file
/// Connects TBL-KeyBinding (issue #21) to the SDL2 backend's key hook (SR-FW-06, SR-INP-09).
#pragma once

#include <optional>

#include "backend_sdl/key_action_lookup.hpp"
#include "input/key_bindings.hpp"

namespace arcade {

/// KeyActionLookup over KeyBindings, so PC keys map as on the board (SR-FW-06).
class BindingLookup final : public KeyActionLookup {
  public:
    /// `bindings` must outlive this object.
    explicit BindingLookup(const KeyBindings& bindings) noexcept : bindings_(bindings) {}

    /// See KeyActionLookup::lookup() (SR-INP-09 to SR-INP-11).
    std::optional<KeyMapping> lookup(InputContext context,
                                     std::uint16_t keyCode) const noexcept override {
        const std::optional<InputEvent> event = bindings_.translateKeyDown(context, keyCode);
        if (!event) {
            return std::nullopt;
        }
        return KeyMapping{event->action, event->ch};
    }

  private:
    const KeyBindings& bindings_;
};

}  // namespace arcade
