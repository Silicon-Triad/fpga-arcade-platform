/// @file
/// App state Initials: section 3.1.1 screen 7 (SR-UI-10).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "arcade/tables.hpp"
#include "engine/screen.hpp"

namespace arcade {

/// Screen of AppState::Initials: types three letters, then raises a request App acts on in the
/// same tick (SR-UI-10). App does the insert and the save, so this screen touches no store.
class InitialsScreen final : public Screen {
  public:
    /// Starts an empty entry for a score of `score` that will be rank `rank` in game `name`.
    void begin(std::string_view name, std::uint32_t score, std::uint8_t rank) noexcept;
    /// Handles Char, Delete and Select; Select counts only with three letters (SR-UI-10).
    void update(const InputFrame& input) noexcept override;
    /// Draws section 3.1.1 screen 7.
    void render(EngineApi& api) noexcept override;

    /// True once if Select was accepted since the last call (SR-UI-10).
    bool takeConfirmed() noexcept;
    /// The letters typed; complete only after takeConfirmed() returned true.
    const std::array<char, INITIALS_LEN>& letters() const noexcept { return letters_; }

  private:
    std::string_view name_;
    std::uint32_t score_ = 0;
    std::uint8_t rank_ = kRankMin;
    std::array<char, INITIALS_LEN> letters_{};
    std::size_t count_ = 0;
    bool confirmed_ = false;
};

}  // namespace arcade
