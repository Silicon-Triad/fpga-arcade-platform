/// @file
/// App state ShutdownConfirm: section 3.1.1 screen 10a (SR-UI-14).
#pragma once

#include <cstdint>

#include "engine/screen.hpp"

namespace arcade {

/// The box over the main menu; App draws the menu first. Select confirms and Back cancels
/// (SR-UI-14).
class ShutdownConfirmScreen final : public Screen {
  public:
    /// What the player answered.
    enum class Request : std::uint8_t { None, Confirm, Cancel };

    /// Handles Select and Back; the first KeyDown of the tick decides (SR-UI-14).
    void update(const InputFrame& input) noexcept override;
    /// Draws the box and hint bar of screen 10a.
    void render(EngineApi& api) noexcept override;
    /// Returns the pending request and clears it.
    Request takeRequest() noexcept;

  private:
    Request request_ = Request::None;
};

}  // namespace arcade
