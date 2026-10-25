/// @file
/// Translation of SDL scancodes to Linux evdev key codes (`KEY_*`), so the SDL2 backend hands
/// the key-binding lookup the same codes the Input Manager reads on the board (SR-FW-06).
#pragma once

#include <SDL2/SDL_scancode.h>

#include <cstdint>
#include <optional>

namespace arcade {

/// Linux `KEY_*` values, copied from `<linux/input-event-codes.h>` so the backend also builds
/// on Windows (QA-03). On Linux, sdl_backend.cpp checks each value against that header.
namespace linux_key {
constexpr std::uint16_t kEsc = 1;          // KEY_ESC
constexpr std::uint16_t k1 = 2;            // KEY_1 (KEY_2 to KEY_9 follow: 3-10)
constexpr std::uint16_t k0 = 11;           // KEY_0
constexpr std::uint16_t kBackspace = 14;   // KEY_BACKSPACE
constexpr std::uint16_t kTab = 15;         // KEY_TAB
constexpr std::uint16_t kQ = 16;           // KEY_Q
constexpr std::uint16_t kW = 17;           // KEY_W
constexpr std::uint16_t kE = 18;           // KEY_E
constexpr std::uint16_t kR = 19;           // KEY_R
constexpr std::uint16_t kT = 20;           // KEY_T
constexpr std::uint16_t kY = 21;           // KEY_Y
constexpr std::uint16_t kU = 22;           // KEY_U
constexpr std::uint16_t kI = 23;           // KEY_I
constexpr std::uint16_t kO = 24;           // KEY_O
constexpr std::uint16_t kP = 25;           // KEY_P
constexpr std::uint16_t kEnter = 28;       // KEY_ENTER
constexpr std::uint16_t kLeftCtrl = 29;    // KEY_LEFTCTRL
constexpr std::uint16_t kA = 30;           // KEY_A
constexpr std::uint16_t kS = 31;           // KEY_S
constexpr std::uint16_t kD = 32;           // KEY_D
constexpr std::uint16_t kF = 33;           // KEY_F
constexpr std::uint16_t kG = 34;           // KEY_G
constexpr std::uint16_t kH = 35;           // KEY_H
constexpr std::uint16_t kJ = 36;           // KEY_J
constexpr std::uint16_t kK = 37;           // KEY_K
constexpr std::uint16_t kL = 38;           // KEY_L
constexpr std::uint16_t kLeftShift = 42;   // KEY_LEFTSHIFT
constexpr std::uint16_t kZ = 44;           // KEY_Z
constexpr std::uint16_t kX = 45;           // KEY_X
constexpr std::uint16_t kC = 46;           // KEY_C
constexpr std::uint16_t kV = 47;           // KEY_V
constexpr std::uint16_t kB = 48;           // KEY_B
constexpr std::uint16_t kN = 49;           // KEY_N
constexpr std::uint16_t kM = 50;           // KEY_M
constexpr std::uint16_t kRightShift = 54;  // KEY_RIGHTSHIFT
constexpr std::uint16_t kSpace = 57;       // KEY_SPACE
constexpr std::uint16_t kRightCtrl = 97;   // KEY_RIGHTCTRL
constexpr std::uint16_t kUp = 103;         // KEY_UP
constexpr std::uint16_t kLeft = 105;       // KEY_LEFT
constexpr std::uint16_t kRight = 106;      // KEY_RIGHT
constexpr std::uint16_t kDown = 108;       // KEY_DOWN
constexpr std::uint16_t kDelete = 111;     // KEY_DELETE
constexpr std::uint16_t kCount = 0x300;    // KEY_CNT: size of a per-key state array
}  // namespace linux_key

/// Linux `KEY_*` code for an SDL scancode, or std::nullopt for a key the backend does not
/// translate (SR-FW-06). Covers A-Z, 0-9, Enter, Esc, Backspace, Tab, Space, Delete, the arrow
/// keys, Shift and Ctrl: every key the board's keyboard must report (SR-INP-01) and more.
std::optional<std::uint16_t> linuxKeyCode(SDL_Scancode scancode) noexcept;

}  // namespace arcade
