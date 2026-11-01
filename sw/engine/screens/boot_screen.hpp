/// @file
/// App state Boot: the boot / splash screen (section 3.1.1 screen 1, App. A appState).
#pragma once

#include "engine/screen.hpp"

namespace arcade {

/// Section 3.1.1 screen 1: logo, subtitle, progress bar, status line and the optional
/// no-keyboard warning.
class BootScreen final : public Screen {
  public:
    /// Sets the bar to `done` of `total` steps, clamped to full (section 3.1.1 screen 1).
    void setProgress(int done, int total) noexcept;
    /// Shows or hides the no-keyboard message on row 60 (section 3.1.1 screen 1).
    void setKeyboardMissing(bool missing) noexcept { keyboardMissing_ = missing; }
    /// Green cells of the bar now drawn (section 3.1.1 screen 1).
    int barCells() const noexcept { return barCells_; }

    void update(const InputFrame& input) noexcept override;
    /// Draws section 3.1.1 screen 1.
    void render(EngineApi& api) noexcept override;

  private:
    int barCells_ = 0;
    bool keyboardMissing_ = false;
};

}  // namespace arcade
