// SDL scancode to Linux KEY_* translation (SR-FW-06).
#include "backend_sdl/key_codes.hpp"

#include <array>
#include <utility>

#if defined(__linux__)
#include <linux/input-event-codes.h>
#endif

namespace arcade {

#if defined(__linux__)
// The copied values must equal the kernel header's, so PC and board codes match (SR-FW-06).
static_assert(linux_key::kEsc == KEY_ESC && linux_key::k1 == KEY_1 && linux_key::k0 == KEY_0);
static_assert(linux_key::kBackspace == KEY_BACKSPACE && linux_key::kTab == KEY_TAB);
static_assert(linux_key::kQ == KEY_Q && linux_key::kW == KEY_W && linux_key::kE == KEY_E &&
              linux_key::kR == KEY_R && linux_key::kT == KEY_T && linux_key::kY == KEY_Y &&
              linux_key::kU == KEY_U && linux_key::kI == KEY_I && linux_key::kO == KEY_O &&
              linux_key::kP == KEY_P);
static_assert(linux_key::kEnter == KEY_ENTER && linux_key::kLeftCtrl == KEY_LEFTCTRL);
static_assert(linux_key::kA == KEY_A && linux_key::kS == KEY_S && linux_key::kD == KEY_D &&
              linux_key::kF == KEY_F && linux_key::kG == KEY_G && linux_key::kH == KEY_H &&
              linux_key::kJ == KEY_J && linux_key::kK == KEY_K && linux_key::kL == KEY_L);
static_assert(linux_key::kLeftShift == KEY_LEFTSHIFT && linux_key::kRightShift == KEY_RIGHTSHIFT);
static_assert(linux_key::kZ == KEY_Z && linux_key::kX == KEY_X && linux_key::kC == KEY_C &&
              linux_key::kV == KEY_V && linux_key::kB == KEY_B && linux_key::kN == KEY_N &&
              linux_key::kM == KEY_M);
static_assert(linux_key::kSpace == KEY_SPACE && linux_key::kRightCtrl == KEY_RIGHTCTRL);
static_assert(linux_key::kUp == KEY_UP && linux_key::kLeft == KEY_LEFT &&
              linux_key::kRight == KEY_RIGHT && linux_key::kDown == KEY_DOWN);
static_assert(linux_key::kDelete == KEY_DELETE && linux_key::kCount == KEY_CNT);
#endif

namespace {

/// Letters in SDL scancode order (SDL_SCANCODE_A to SDL_SCANCODE_Z are consecutive).
constexpr std::array<std::uint16_t, 26> kLetters = {
    linux_key::kA, linux_key::kB, linux_key::kC, linux_key::kD, linux_key::kE, linux_key::kF,
    linux_key::kG, linux_key::kH, linux_key::kI, linux_key::kJ, linux_key::kK, linux_key::kL,
    linux_key::kM, linux_key::kN, linux_key::kO, linux_key::kP, linux_key::kQ, linux_key::kR,
    linux_key::kS, linux_key::kT, linux_key::kU, linux_key::kV, linux_key::kW, linux_key::kX,
    linux_key::kY, linux_key::kZ};

/// Other translated keys.
constexpr std::array<std::pair<SDL_Scancode, std::uint16_t>, 15> kOthers = {{
    {SDL_SCANCODE_ESCAPE, linux_key::kEsc},
    {SDL_SCANCODE_0, linux_key::k0},
    {SDL_SCANCODE_BACKSPACE, linux_key::kBackspace},
    {SDL_SCANCODE_TAB, linux_key::kTab},
    {SDL_SCANCODE_RETURN, linux_key::kEnter},
    {SDL_SCANCODE_SPACE, linux_key::kSpace},
    {SDL_SCANCODE_DELETE, linux_key::kDelete},
    {SDL_SCANCODE_UP, linux_key::kUp},
    {SDL_SCANCODE_DOWN, linux_key::kDown},
    {SDL_SCANCODE_LEFT, linux_key::kLeft},
    {SDL_SCANCODE_RIGHT, linux_key::kRight},
    {SDL_SCANCODE_LSHIFT, linux_key::kLeftShift},
    {SDL_SCANCODE_RSHIFT, linux_key::kRightShift},
    {SDL_SCANCODE_LCTRL, linux_key::kLeftCtrl},
    {SDL_SCANCODE_RCTRL, linux_key::kRightCtrl},
}};

}  // namespace

std::optional<std::uint16_t> linuxKeyCode(SDL_Scancode scancode) noexcept {
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return kLetters[static_cast<std::size_t>(scancode - SDL_SCANCODE_A)];
    }
    // SDL_SCANCODE_1 to _9 are consecutive, as are KEY_1 to KEY_9.
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return static_cast<std::uint16_t>(linux_key::k1 + (scancode - SDL_SCANCODE_1));
    }
    for (const auto& [sdl, code] : kOthers) {
        if (sdl == scancode) {
            return code;
        }
    }
    return std::nullopt;
}

}  // namespace arcade
