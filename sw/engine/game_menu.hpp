/// @file
/// The main menu, built from TBL-GameRegistry at start-up (SR-FW-04, SR-UI-01 to SR-UI-03).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "arcade/tables.hpp"
#include "engine/screen.hpp"

namespace arcade {

/// Returns a game's best score for the menu; #76 supplies the real lookup.
using BestScoreFn = std::uint32_t (*)(std::uint8_t gameId);

/// One game of the menu: its registry row and the object the row's factory made.
struct MenuGame {
    const GameRegistryRow* row;  ///< Registry row (SR-FW-04).
    Game* game;                  ///< Created at start-up (SR-FW-03).
};

/// Screen 2. Lists every game by menu_order, then SCORES; Up and Down wrap (SR-UI-01). Select
/// raises a request that App acts on in the same tick (SR-UI-03).
class MenuScreen final : public Screen {
  public:
    /// What Select asked for.
    enum class Request : std::uint8_t { None, StartGame, Scores };

    /// Sorts `games` by menu_order. Start-up only: it allocates (R-4.1.6).
    MenuScreen(std::vector<MenuGame> games, BestScoreFn bestScore);

    /// Handles Up, Down and Select, and runs the selected game's preview (SR-UI-01 to SR-UI-03).
    void update(const InputFrame& input) noexcept override;
    /// Draws section 3.1.1 screen 2.
    void render(EngineApi& api) noexcept override;

    /// Returns the pending request and clears it.
    Request takeRequest() noexcept;
    /// Number of entries: the games plus SCORES (SR-UI-01).
    std::size_t entryCount() const noexcept { return games_.size() + 1; }
    /// Name of entry `index`; the last entry is SCORES (SR-UI-01).
    std::string_view entryName(std::size_t index) const noexcept;
    /// The highlighted entry (App. A menuSelection).
    std::size_t selection() const noexcept { return selection_; }
    /// The selected game, or nullptr on SCORES.
    const MenuGame* selectedGame() const noexcept;

  private:
    void movePreview() noexcept;

    std::vector<MenuGame> games_;
    BestScoreFn bestScore_;
    std::size_t selection_ = 0;
    Request request_ = Request::None;
    std::uint32_t previewTicks_ = 0;
};

}  // namespace arcade
