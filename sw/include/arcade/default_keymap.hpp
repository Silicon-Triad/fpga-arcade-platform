// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// TBL-KeyBinding default rows, compiled in (srs-muhammad.md Chapter 5, "Default bindings").
/// The row type is KeyBindingRow in tables.hpp; the Input Manager copies these rows and lets
/// /etc/arcade/keymap.conf replace individual ones (SR-INP-09 to SR-INP-11, SC-05).
///
/// Key codes are Linux evdev KEY_* values, so this header includes <linux/input.h>. Only
/// sw/input/ includes it; game code never does (QA-03), which is why the rows are not in
/// tables.hpp (agreed on #21).
#pragma once

#include <linux/input.h>

#include <array>
#include <cstddef>

#include "arcade/tables.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Number of default TBL-KeyBinding rows: Global 1, Menu 5, Snake 6, Breakout 5, Paused 2,
/// GameOver 2, Initials 28, HighScores 3, ShutdownConfirm 2 (Chapter 5, "Default bindings").
constexpr std::size_t kDefaultKeyBindingCount = 54;

/// TBL-KeyBinding default rows, in the order of the Chapter 5 table. A key is looked up in the
/// current context first and then in Global (SR-INP-09).
constexpr std::array<KeyBindingRow, kDefaultKeyBindingCount> kDefaultKeyBindings{{
    // Global: M -> Mute.
    {InputContext::Global, KEY_M, Action::Mute},
    // Menu: Up -> Up, Down -> Down, Enter -> Select, H -> Select (SCORES),
    // Esc -> Back (held 3 s: shutdown, SR-UI-14).
    {InputContext::Menu, KEY_UP, Action::Up},
    {InputContext::Menu, KEY_DOWN, Action::Down},
    {InputContext::Menu, KEY_ENTER, Action::Select},
    {InputContext::Menu, KEY_H, Action::Select},
    {InputContext::Menu, KEY_ESC, Action::Back},
    // Snake: arrows -> Up, Down, Left, Right; P -> Pause; Esc -> Back.
    {InputContext::Snake, KEY_UP, Action::Up},
    {InputContext::Snake, KEY_DOWN, Action::Down},
    {InputContext::Snake, KEY_LEFT, Action::Left},
    {InputContext::Snake, KEY_RIGHT, Action::Right},
    {InputContext::Snake, KEY_P, Action::Pause},
    {InputContext::Snake, KEY_ESC, Action::Back},
    // Breakout: Left, Right; Space -> Launch; P -> Pause; Esc -> Back.
    {InputContext::Breakout, KEY_LEFT, Action::Left},
    {InputContext::Breakout, KEY_RIGHT, Action::Right},
    {InputContext::Breakout, KEY_SPACE, Action::Launch},
    {InputContext::Breakout, KEY_P, Action::Pause},
    {InputContext::Breakout, KEY_ESC, Action::Back},
    // Paused: P -> Pause; Esc -> Back.
    {InputContext::Paused, KEY_P, Action::Pause},
    {InputContext::Paused, KEY_ESC, Action::Back},
    // GameOver: Enter -> Select; Esc -> Back.
    {InputContext::GameOver, KEY_ENTER, Action::Select},
    {InputContext::GameOver, KEY_ESC, Action::Back},
    // Initials: A-Z -> Char (overrides Global M); Backspace -> Delete; Enter -> Select.
    {InputContext::Initials, KEY_A, Action::Char},
    {InputContext::Initials, KEY_B, Action::Char},
    {InputContext::Initials, KEY_C, Action::Char},
    {InputContext::Initials, KEY_D, Action::Char},
    {InputContext::Initials, KEY_E, Action::Char},
    {InputContext::Initials, KEY_F, Action::Char},
    {InputContext::Initials, KEY_G, Action::Char},
    {InputContext::Initials, KEY_H, Action::Char},
    {InputContext::Initials, KEY_I, Action::Char},
    {InputContext::Initials, KEY_J, Action::Char},
    {InputContext::Initials, KEY_K, Action::Char},
    {InputContext::Initials, KEY_L, Action::Char},
    {InputContext::Initials, KEY_M, Action::Char},
    {InputContext::Initials, KEY_N, Action::Char},
    {InputContext::Initials, KEY_O, Action::Char},
    {InputContext::Initials, KEY_P, Action::Char},
    {InputContext::Initials, KEY_Q, Action::Char},
    {InputContext::Initials, KEY_R, Action::Char},
    {InputContext::Initials, KEY_S, Action::Char},
    {InputContext::Initials, KEY_T, Action::Char},
    {InputContext::Initials, KEY_U, Action::Char},
    {InputContext::Initials, KEY_V, Action::Char},
    {InputContext::Initials, KEY_W, Action::Char},
    {InputContext::Initials, KEY_X, Action::Char},
    {InputContext::Initials, KEY_Y, Action::Char},
    {InputContext::Initials, KEY_Z, Action::Char},
    {InputContext::Initials, KEY_BACKSPACE, Action::Delete},
    {InputContext::Initials, KEY_ENTER, Action::Select},
    // HighScores: Left, Right (switch game, SR-UI-12); Esc -> Back.
    {InputContext::HighScores, KEY_LEFT, Action::Left},
    {InputContext::HighScores, KEY_RIGHT, Action::Right},
    {InputContext::HighScores, KEY_ESC, Action::Back},
    // ShutdownConfirm: Enter -> Select; Esc -> Back.
    {InputContext::ShutdownConfirm, KEY_ENTER, Action::Select},
    {InputContext::ShutdownConfirm, KEY_ESC, Action::Back},
}};

}  // namespace arcade
