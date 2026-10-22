// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// The Arcade API: the only interface games use to draw, present frames, read input and play
/// sounds (Glossary "Arcade API", SR-FW-05).
#pragma once

#include <cstdint>
#include <string_view>

#include "arcade/types.hpp"

namespace arcade {

/// Abstract Arcade API. Each backend (FPGA or SDL2, chosen at start-up) implements it, so game
/// code is identical for both (SR-FW-05, Glossary "Backend"). Drawing calls write only to the
/// shadow tilemap (SR-HAL-03, SR-HAL-04); present() publishes it. No call throws (R-4.1.5).
class ArcadeApi {
  public:
    virtual ~ArcadeApi() = default;

    /// Sets the cell at (col, row) to `tile` (App. A Tilemap drawing, SR-HAL-03).
    /// @return OutOfRange, and nothing drawn, if the cell is outside the 128 x 96 grid.
    virtual ApiStatus setTile(int col, int row, TileIndex tile) noexcept = 0;

    /// Fills the width x height rectangle with top-left cell (col, row) with `tile`
    /// (App. A Tilemap drawing).
    /// @return OutOfRange, and nothing drawn, if any part lies outside the grid.
    virtual ApiStatus fillRect(int col, int row, int width, int height,
                               TileIndex tile) noexcept = 0;

    /// Draws `text` with its first character's top-left cell at (col, row) (SR-FW-12):
    /// scale X1 uses one font tile per character, scale X4 uses 4 x 4 quadrant-mosaic tiles.
    /// @return OutOfRange if the text does not fit; InvalidArgument for a character with no
    /// font tile. Nothing is drawn in either case.
    virtual ApiStatus drawText(int col, int row, std::string_view text,
                               TextScale scale) noexcept = 0;

    /// Sets every cell of the grid to `tile` (App. A Tilemap drawing).
    virtual void clear(TileIndex tile) noexcept = 0;

    /// Ends the frame: publishes the shadow tilemap and returns at the next display refresh,
    /// which locks the game loop to 60 ticks per second (SR-FW-02, SR-HAL-08).
    /// @return BackendError if the bank swap is not confirmed before the HAL timeout.
    virtual ApiStatus present() noexcept = 0;

    /// Returns this tick's input; never blocks (SR-FW-02, SR-INP-02, App. A Action events).
    /// At most kMaxEventsPerTick events are returned, oldest first; any further events stay
    /// queued, in order, for the next call. No event is dropped.
    virtual InputFrame pollInput() noexcept = 0;

    /// Starts sound effect `effectId`, replacing any effect still playing (SR-HAL-11,
    /// TBL-SoundEffect). Every call is accepted, even when muted or silent (SR-FW-07).
    virtual void playSound(EffectId effectId) noexcept = 0;

    /// Mutes sound when `on` is true, unmutes it when false (SR-FW-11, SR-HAL-12). Every call is
    /// accepted (SR-FW-07).
    virtual void setMute(bool on) noexcept = 0;

    /// Loads all 256 tile patterns and the 16-colour palette, in TBL-TilePattern and
    /// TBL-Palette format (SR-FW-01, SR-REN-04). Called once, by the Game Engine at start-up,
    /// with one combined set: the font, the 16 quadrant-mosaic tiles and every game's tiles.
    /// Games never call it. Display enable is set only after it succeeds (SR-HAL-13).
    virtual ApiStatus loadTiles(const TileSet& patterns, const Palette& palette) noexcept = 0;

    /// Number of frames displayed since power-on (FPGA Frame Counter register, SR-HAL-06) or
    /// since the SDL2 window opened. It counts display refreshes, not present() calls, so it
    /// equals the tick count only when no tick overruns its frame (SR-FW-02).
    virtual std::uint32_t frameCount() const noexcept = 0;
};

}  // namespace arcade
