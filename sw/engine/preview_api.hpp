/// @file
/// Arcade API view that shows a game inside the menu's preview panel (SR-UI-02).
#pragma once

#include "arcade/arcade_api.hpp"

namespace arcade {

/// First and last panel columns and rows a preview may draw (section 3.1.1 screen 2: inside the
/// green frame on columns 60-121 and rows 24-77, above the best-score line on row 74).
constexpr int kPreviewFirstCol = 61;
constexpr int kPreviewLastCol = 120;
constexpr int kPreviewFirstRow = 25;
constexpr int kPreviewLastRow = 72;

/// Moves a game's drawing into the panel, cropping the rest, and stops the preview from making
/// sound or touching the frame (SR-UI-02). No call throws (R-4.1.5).
class PreviewApi final : public ArcadeApi {
  public:
    /// Forwards visible cells to `target`.
    explicit PreviewApi(ArcadeApi& target) noexcept : target_(target) {}

    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override;
    ApiStatus fillRect(int col, int row, int width, int height, TileIndex tile) noexcept override;
    /// Text is not shown in the preview.
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override;
    void clear(TileIndex tile) noexcept override;
    ApiStatus present() noexcept override;
    InputFrame pollInput() noexcept override;
    void playSound(EffectId effectId) noexcept override;
    void setMute(bool on) noexcept override;
    ApiStatus loadTiles(const TileSet& patterns, const Palette& palette) noexcept override;
    std::uint32_t frameCount() const noexcept override;

  private:
    ArcadeApi& target_;
};

}  // namespace arcade
