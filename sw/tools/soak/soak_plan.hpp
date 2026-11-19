#pragma once

// Soak driver key plan (QA-05, issue #51): the part that needs no device, so it can be
// unit-tested. The program that sends the plan through uinput is soak_main.cpp.

#include <linux/input.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arcade::soak {

/// QA-05: 2 hours of continuous play.
constexpr std::uint32_t kQa05Minutes = 120;

/// Keys the virtual keyboard reports. SR-INP-01: the Input Manager only selects a device that
/// reports A-Z, Enter, Esc and the four arrows; Space (Breakout Launch), P (Pause) and
/// Backspace are added for TBL-KeyBinding. M (Mute) is reported but never pressed.
constexpr std::array<std::uint16_t, 34> kSoakKeys = {
    KEY_A,   KEY_B,  KEY_C,    KEY_D,    KEY_E,     KEY_F,     KEY_G,        KEY_H, KEY_I,
    KEY_J,   KEY_K,  KEY_L,    KEY_M,    KEY_N,     KEY_O,     KEY_P,        KEY_Q, KEY_R,
    KEY_S,   KEY_T,  KEY_U,    KEY_V,    KEY_W,     KEY_X,     KEY_Y,        KEY_Z, KEY_ENTER,
    KEY_ESC, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_BACKSPACE};

/// Plan settings. Only `minutes` comes from the SRS (QA-05); the rest are test choices.
struct PlanOptions {
    std::uint32_t minutes = kQa05Minutes;  ///< total length
    std::uint32_t seed = 1;                ///< pseudo-random key pattern, repeatable
    std::uint32_t playSeconds = 180;       ///< scripted play per round
    std::uint32_t endSeconds = 40;         ///< keys that let the game end (no steering)
};

/// One key event of the plan, `atMs` after the start.
struct Step {
    std::uint64_t atMs = 0;
    std::uint16_t key = 0;
    std::int32_t value = 0;  ///< 1 press, 0 release (SR-INP-05, SR-INP-06)
    const char* note = "";   ///< what the step is for, for the log
};

/// Menu entries in menu order: SNAKE (0), BREAKOUT (1), SCORES (2) (TBL-GameRegistry
/// menu_order, then SCORES; the selection wraps, SR-UI-01).
constexpr std::uint32_t kMenuEntries = 3;
/// Game of round `r`: Snake on even rounds, Breakout on odd ones (QA-05 "alternating").
constexpr std::uint32_t gameOfRound(std::uint32_t r) { return r % 2; }
/// Initials are typed in rounds 2, 3, 6, 7, ... so each game saves the file every other time.
constexpr bool initialsInRound(std::uint32_t r) { return (r / 2) % 2 == 1; }

/// Builds the whole plan. Rounds alternate Snake and Breakout until the next round would not
/// fit in `minutes`. Each round: select the game from the menu, play, let the game end,
/// sometimes type initials and Enter, then Esc back to the menu (see README for why every
/// path returns to the menu). Every press is released before the next press.
std::vector<Step> makePlan(const PlanOptions& opt);

/// Number of whole rounds makePlan() fits in `opt.minutes`.
std::uint32_t roundCount(const PlanOptions& opt);

/// Short name of `key` for the log ("KEY_UP"), or "KEY_?" if not in kSoakKeys.
const char* keyName(std::uint16_t key);

/// Command-line options of `soak_driver`.
struct Options {
    PlanOptions plan;
    std::string device = "/dev/uinput";  ///< same default as sw/tools/uinput (#23)
    std::uint32_t delaySeconds = 15;     ///< time to start arcade after the device appears
    bool dryRun = false;                 ///< print the plan, open no device
    bool help = false;
};

/// Parses `--minutes N` (1..1440), `--seed N`, `--play N` (10..3600), `--device PATH`,
/// `--delay N` (0..600), `--dry-run`, `--help`. std::nullopt and `error` on a bad argument.
std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error);

/// Usage text.
std::string usage();

}  // namespace arcade::soak
