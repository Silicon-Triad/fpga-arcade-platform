/// @file
/// The application state machine (SRS Chapter 5, App. A appState).
#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <vector>

#include "arcade/engine_api.hpp"
#include "arcade/tables.hpp"
#include "engine/app_state.hpp"
#include "engine/game_menu.hpp"
#include "engine/screen.hpp"
#include "engine/screens/boot_screen.hpp"
#include "engine/screens/game_over_screen.hpp"
#include "engine/screens/hardware_not_ready_screen.hpp"
#include "engine/screens/high_scores_screen.hpp"
#include "engine/screens/initials_screen.hpp"
#include "engine/screens/keyboard_lost_screen.hpp"
#include "engine/screens/paused_screen.hpp"
#include "engine/screens/playing_screen.hpp"
#include "engine/screens/shutdown_confirm_screen.hpp"
#include "engine/screens/shutting_down_screen.hpp"

namespace arcade {

/// The active screen of the game loop: holds the state, runs that state's screen and makes the
/// transitions. It knows games only through the registry and Game (SR-FW-03, QA-02).
class App final : public Screen {
  public:
    /// Calls each row's factory once and builds the menu (SR-FW-04). Start-up only (R-4.1.6).
    /// The state starts at Boot (Chapter 5).
    App(EngineApi& api, const GameRegistryRow* rows, std::size_t rowCount, BestScoreFn bestScore);

    /// Moves to `state` and sets its key-binding context (SR-INP-09).
    void setState(AppState state) noexcept;
    /// Start-up reports it is done: Boot goes to Menu (SR-FW-01). Does nothing in other states.
    /// Until then Boot stays, so the caller decides the progress and the timing.
    void bootFinished() noexcept;
    /// The Boot screen, for the caller to drive (SR-FW-01).
    BootScreen& boot() noexcept { return boot_; }
    /// The current state.
    AppState state() const noexcept { return state_; }
    /// The screen of `state`; never nullptr.
    Screen* screenFor(AppState state) noexcept;
    /// The menu.
    MenuScreen& menu() noexcept { return *menu_; }
    /// The Playing screen.
    const PlayingScreen& playing() const noexcept { return playing_; }

    /// Runs the current screen and acts on what it requested (SR-UI-03).
    void update(const InputFrame& input) noexcept override;
    /// Draws the current screen.
    void render(EngineApi& api) noexcept override;

  private:
    EngineApi& api_;
    std::vector<std::unique_ptr<Game>> games_;
    std::unique_ptr<MenuScreen> menu_;
    BootScreen boot_;
    HardwareNotReadyScreen hardwareNotReady_;
    HighScoresScreen highScores_;
    ShutdownConfirmScreen shutdownConfirm_;
    ShuttingDownScreen shuttingDown_;
    PlayingScreen playing_;
    PausedScreen paused_;
    KeyboardLostScreen keyboardLost_;
    GameOverScreen gameOver_;
    InitialsScreen initials_;
    AppState state_ = AppState::Boot;
    InputContext playContext_ = InputContext::Snake;
};

}  // namespace arcade
