// Unit test for the key hook of the SDL2 backend (issue #16, SR-FW-06, SR-INP-09).
#include "app/binding_lookup.hpp"

#include <gtest/gtest.h>

#include "backend_sdl/key_codes.hpp"

namespace arcade {
namespace {

TEST(BindingLookup, SR_INP_09_UsesTheBindingTable) {
    const KeyBindings bindings;
    const BindingLookup lookup(bindings);
    const auto enter = lookup.lookup(InputContext::Menu, linux_key::kEnter);
    EXPECT_EQ(enter.value_or(KeyMapping{Action::Back, 0}).action, Action::Select);
    EXPECT_FALSE(lookup.lookup(InputContext::Snake, linux_key::kEnter).has_value());
}

}  // namespace
}  // namespace arcade
