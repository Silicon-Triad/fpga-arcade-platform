// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// Shared types of the Arcade API and the game interface: grid constants, actions, input
/// events, sound effect IDs and tile indices (SR-FW-03, SR-FW-05, Appendix A).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arcade {

/// Tilemap width in tiles (SR-FW-06, TBL-Tilemap cell_x 0-127).
constexpr int kGridCols = 128;
/// Tilemap height in tiles (SR-FW-06, TBL-Tilemap cell_y 0-95).
constexpr int kGridRows = 96;
/// Tile width and height in pixels (TBL-TilePattern: 8x8 pixels).
constexpr int kTilePx = 8;
/// Number of tile patterns (TBL-TilePattern tile_index 0-255, 256 rows).
constexpr std::size_t kTileCount = 256;
/// Bytes per tile pattern: 4 bits/pixel, low nibble = left pixel (TBL-TilePattern).
constexpr std::size_t kTilePatternBytes = 32;
/// Number of palette colours (TBL-Palette palette_index 0-15, 16 rows).
constexpr std::size_t kPaletteSize = 16;

/// Index of a tile pattern written into a tilemap cell (TBL-Tilemap tile_index, UINT8).
using TileIndex = std::uint8_t;

/// One tile pattern in TBL-TilePattern format: 8x8 pixels, 4-bit palette indices packed two
/// per byte, low nibble = left pixel (SR-REN-04, TBL-TilePattern).
using TilePattern = std::array<std::uint8_t, kTilePatternBytes>;
/// All 256 tile patterns (TBL-TilePattern).
using TileSet = std::array<TilePattern, kTileCount>;
/// Palette colours packed as 0x00RRGGBB (TBL-Palette rgb, UINT32).
using Palette = std::array<std::uint32_t, kPaletteSize>;

/// Logical player action a key maps to (TBL-KeyBinding action, SR-INP-09, App. A Action events).
enum class Action : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Select,
    Back,
    Pause,
    Launch,
    Char,  ///< A letter A-Z; the letter is in InputEvent::ch (Initials screen, SR-UI-10).
    Delete,
    Mute,  ///< Toggles sound through setMute() (SR-FW-11).
};
/// Number of Action values; size of the held-state array (TBL-KeyBinding).
constexpr std::size_t kActionCount = 11;

/// Sound effect played by playSound() (TBL-SoundEffect effect_id 0-6, SR-HAL-11).
enum class EffectId : std::uint8_t {
    Eat = 0,
    BrickHit = 1,
    PaddleHit = 2,
    LifeLost = 3,
    GameOver = 4,
    MenuMove = 5,
    MenuSelect = 6,
};
/// Number of sound effects (TBL-SoundEffect, ids 0-6).
constexpr std::size_t kEffectCount = 7;

/// Kind of input event delivered by pollInput() (App. A Inputs and outputs; SR-INP-08,
/// SR-INP-14, SR-UI-11).
enum class InputEventType : std::uint8_t {
    KeyDown,        ///< A bound key was pressed.
    KeyUp,          ///< A bound key was released.
    KeyboardLost,   ///< The keyboard was removed (SR-INP-14, SR-UI-11).
    KeyboardFound,  ///< A keyboard was found again (SR-INP-14, SR-UI-11).
};

/// One input event (App. A Action events, KeyboardLost / KeyboardFound).
struct InputEvent {
    InputEventType type = InputEventType::KeyDown;  ///< What happened.
    Action action = Action::Up;  ///< Mapped action; only meaningful for KeyDown and KeyUp.
    char ch = 0;                 ///< 'A'-'Z' when action is Action::Char, else 0.
};

/// Maximum events delivered in one InputFrame.
/// DESIGN CHOICE, NOT AN SRS VALUE: derived from USB polling, about 2 keyboard reports per
/// 60 Hz tick x 6 keys x down/up = 24, rounded up to 32. Events beyond this stay queued for the
/// next tick, in order; none are dropped.
constexpr std::size_t kMaxEventsPerTick = 32;

/// Everything pollInput() returns for one tick: the events since the last tick, in arrival
/// order, plus the held state of every action (SR-FW-02, SR-INP-08, App. A Held-key state).
struct InputFrame {
    std::array<InputEvent, kMaxEventsPerTick> events{};  ///< events[0 .. count-1] are valid.
    std::size_t count = 0;                               ///< Number of valid events.
    std::array<bool, kActionCount> held{};  ///< held[action] is true while a key bound to it is
                                            ///< down, e.g. for paddle movement (SR-GAM-09).
};

/// Text size for drawText() (SR-FW-12): one font tile per character, or 4x4 tiles per character
/// built from the 16 quadrant-mosaic tiles.
enum class TextScale : std::uint8_t {
    x1 = 1,
    x4 = 4,
};

/// Result of an Arcade API call; the API never throws (R-4.1.5).
enum class ApiStatus : std::uint8_t {
    Ok,               ///< Done.
    OutOfRange,       ///< A coordinate or size lies outside the 128 x 96 grid; nothing drawn.
    InvalidArgument,  ///< An argument is not allowed, e.g. a character with no font tile.
    BackendError,     ///< The backend failed (for example a HAL error, SR-HAL-02).
};

}  // namespace arcade
