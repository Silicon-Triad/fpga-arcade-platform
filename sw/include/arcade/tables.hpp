// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// Row types and constants of the software tables: TBL-GameRegistry, TBL-HighScore,
/// TBL-KeyBinding and TBL-SoundEffect (SRS Chapter 5), plus the complete TBL-SoundEffect data
/// (srs-huzaifa-rev3.md section 5).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "arcade/game.hpp"
#include "arcade/types.hpp"

namespace arcade {

// ---------------------------------------------------------------------------------------------
// Constants (Appendix A)
// ---------------------------------------------------------------------------------------------

/// Highest displayable score, 6 digits (SCORE_MAX, SR-UI-04; TBL-HighScore score).
constexpr std::uint32_t SCORE_MAX = 999'999;
/// High-score table size per game (HISCORE_SLOTS, SR-FW-10; TBL-HighScore rank 1-10).
constexpr std::size_t HISCORE_SLOTS = 10;
/// Initials length, letters A-Z (INITIALS_LEN, SR-UI-10; TBL-HighScore initials CHAR(3)).
constexpr std::size_t INITIALS_LEN = 3;
/// High-score file path (HISCORE_FILE, decision D2, SR-FW-08, SR-FW-09).
constexpr std::string_view HISCORE_FILE = "/var/lib/arcade/highscores.csv";

/// Lowest allowed tone frequency in Hz (TONE_FREQ_MIN, SR-AUD-04, SR-HAL-11).
constexpr std::uint16_t TONE_FREQ_MIN = 100;
/// Highest allowed tone frequency in Hz (TONE_FREQ_MAX, SR-AUD-04, SR-HAL-11).
constexpr std::uint16_t TONE_FREQ_MAX = 4'000;
/// Shortest allowed tone duration in ms (TONE_DURATION_MIN, SR-AUD-05, SR-HAL-11).
constexpr std::uint16_t TONE_DURATION_MIN = 10;
/// Longest allowed tone duration in ms (TONE_DURATION_MAX, SR-AUD-05, SR-HAL-11).
constexpr std::uint16_t TONE_DURATION_MAX = 2'000;

/// Game-loop ticks per second, locked to vsync (TICK_RATE, SR-FW-02).
constexpr std::uint16_t TICK_RATE = 60;

/// Converts a tone duration in ms to whole game ticks, rounded to nearest, as the
/// TBL-SoundEffect tick counts are (2 ticks = 33 ms). The HAL uses it to write the next note
/// in the tick the previous one ends (SR-HAL-11). Valid for 0 to TONE_DURATION_MAX.
constexpr std::uint8_t msToTicks(std::uint16_t ms) noexcept {
    return static_cast<std::uint8_t>((ms * TICK_RATE + 500) / 1000);
}

// ---------------------------------------------------------------------------------------------
// TBL-GameRegistry
// ---------------------------------------------------------------------------------------------

/// Release-1.0 game id of Snake (TBL-GameRegistry rows: (0, "SNAKE", 0)).
constexpr std::uint8_t SNAKE = 0;
/// Release-1.0 game id of Breakout (TBL-GameRegistry rows: (1, "BREAKOUT", 1)).
constexpr std::uint8_t BREAKOUT = 1;

/// Longest game name (TBL-GameRegistry name VARCHAR(16)).
constexpr std::size_t kGameNameMax = 16;

/// Creates a new object implementing the game interface (TBL-GameRegistry factory, SR-FW-03).
/// Called once per game at start-up, never inside a game tick (R-4.1.6).
using GameFactory = std::unique_ptr<Game> (*)();

/// One TBL-GameRegistry row; compiled in, read-only (SR-FW-04, SR-UI-01, QA-02).
struct GameRegistryRow {
    std::uint8_t game_id;     ///< PK.
    std::string_view name;    ///< Unique, at most kGameNameMax characters; shown in menu and HUD.
    std::uint8_t menu_order;  ///< Unique; position in the menu.
    GameFactory factory;      ///< Creates the game (SR-FW-03).
};

// ---------------------------------------------------------------------------------------------
// TBL-HighScore
// ---------------------------------------------------------------------------------------------

/// Lowest rank (TBL-HighScore rank 1-10).
constexpr std::uint8_t kRankMin = 1;
/// Highest rank, equal to HISCORE_SLOTS (TBL-HighScore rank 1-10).
constexpr std::uint8_t kRankMax = static_cast<std::uint8_t>(HISCORE_SLOTS);

/// One TBL-HighScore row; one CSV line `game_id,rank,initials,score,seq` in HISCORE_FILE
/// (SR-FW-08 to SR-FW-10, SR-UI-10).
struct HighScoreRow {
    std::uint8_t game_id;                     ///< PK with rank; FK to TBL-GameRegistry.
    std::uint8_t rank;                        ///< PK with game_id; kRankMin to kRankMax.
    std::array<char, INITIALS_LEN> initials;  ///< Exactly three letters A-Z, no terminator.
    std::uint32_t score;                      ///< 0 to SCORE_MAX; ranks sorted descending.
    std::uint32_t seq;                        ///< Entry order; on a tie, older ranks higher.
};

/// Column positions of a TBL-HighScore CSV line `game_id,rank,initials,score,seq` (TBL-HighScore).
enum class HighScoreColumn : std::uint8_t {
    GameId = 0,
    Rank = 1,
    Initials = 2,
    Score = 3,
    Seq = 4,
};
/// Number of columns in a TBL-HighScore CSV line (TBL-HighScore).
constexpr std::size_t kHighScoreColumnCount = 5;
/// Column separator of HISCORE_FILE (TBL-HighScore).
constexpr char kHighScoreSeparator = ',';

// ---------------------------------------------------------------------------------------------
// TBL-KeyBinding
// ---------------------------------------------------------------------------------------------

/// One TBL-KeyBinding row; a key is looked up in the current context, then Global
/// (SR-INP-09 to SR-INP-11, SC-05).
struct KeyBindingRow {
    InputContext context;    ///< PK with key_code.
    std::uint16_t key_code;  ///< PK with context; Linux evdev code KEY_*.
    Action action;           ///< One action per key per context.
};

// ---------------------------------------------------------------------------------------------
// TBL-SoundEffect
// ---------------------------------------------------------------------------------------------

/// Longest effect name (TBL-SoundEffect name VARCHAR(12)).
constexpr std::size_t kEffectNameMax = 12;
/// Lowest note number (TBL-SoundEffect note_no 1-4).
constexpr std::uint8_t kNoteNoMin = 1;
/// Highest note number (TBL-SoundEffect note_no 1-4).
constexpr std::uint8_t kNoteNoMax = 4;
/// Longest total length of one effect in ms (srs-huzaifa-rev3.md section 5, "Effect length").
constexpr std::uint16_t kEffectTotalMaxMs = 500;

/// One TBL-SoundEffect row: one note of one effect (SR-HAL-11, SR-AUD-04, SR-AUD-05).
struct SoundNote {
    EffectId effect_id;     ///< PK with note_no; 0-6.
    std::string_view name;  ///< Same for all notes of one effect; at most kEffectNameMax chars.
    std::uint8_t note_no;   ///< PK with effect_id; kNoteNoMin to kNoteNoMax, play order.
    std::uint16_t freq_hz;  ///< TONE_FREQ_MIN to TONE_FREQ_MAX (SR-AUD-04).
    std::uint16_t dur_ms;   ///< TONE_DURATION_MIN to TONE_DURATION_MAX (SR-AUD-05); whole ticks.
};

/// Number of TBL-SoundEffect rows (srs-huzaifa-rev3.md section 5 contents: 14 notes).
constexpr std::size_t kSoundNoteCount = 14;

/// TBL-SoundEffect contents, copied from srs-huzaifa-rev3.md section 5 and srs-muhammad.md
/// Chapter 5, ordered by effect_id then note_no. Ticks are 1/60 s (TICK_RATE, SR-FW-02).
constexpr std::array<SoundNote, kSoundNoteCount> kSoundEffectTable{{
    {EffectId::Eat, "Eat", 1, 784, 33},                  // G5, 2 ticks
    {EffectId::Eat, "Eat", 2, 1'047, 50},                // C6, 3 ticks
    {EffectId::BrickHit, "BrickHit", 1, 1'319, 33},      // E6, 2 ticks
    {EffectId::PaddleHit, "PaddleHit", 1, 523, 33},      // C5, 2 ticks
    {EffectId::LifeLost, "LifeLost", 1, 392, 100},       // G4, 6 ticks
    {EffectId::LifeLost, "LifeLost", 2, 330, 100},       // E4, 6 ticks
    {EffectId::LifeLost, "LifeLost", 3, 262, 150},       // C4, 9 ticks
    {EffectId::GameOver, "GameOver", 1, 523, 117},       // C5, 7 ticks
    {EffectId::GameOver, "GameOver", 2, 392, 117},       // G4, 7 ticks
    {EffectId::GameOver, "GameOver", 3, 330, 117},       // E4, 7 ticks
    {EffectId::GameOver, "GameOver", 4, 262, 133},       // C4, 8 ticks
    {EffectId::MenuMove, "MenuMove", 1, 880, 33},        // A5, 2 ticks
    {EffectId::MenuSelect, "MenuSelect", 1, 659, 50},    // E5, 3 ticks
    {EffectId::MenuSelect, "MenuSelect", 2, 1'047, 83},  // C6, 5 ticks
}};

}  // namespace arcade
