#include "engine/app.hpp"

#include <optional>

namespace arcade {
namespace {

// TBL-KeyBinding context of `state`; nullopt for states with no keys (section 3.1.1).
std::optional<InputContext> contextFor(AppState state, InputContext playContext) noexcept {
    switch (state) {
        case AppState::Menu:
            return InputContext::Menu;
        case AppState::HighScores:
            return InputContext::HighScores;
        case AppState::ShutdownConfirm:
            return InputContext::ShutdownConfirm;
        case AppState::Playing:
            return playContext;
        case AppState::Paused:
            return InputContext::Paused;
        case AppState::GameOver:
            return InputContext::GameOver;
        case AppState::Initials:
            return InputContext::Initials;
        default:
            return std::nullopt;
    }
}

// Stand-in so that no unfinished screen traps the player (SR-UI-07, SR-UI-12): Back returns to
// the menu. Each issue removes its state when it implements the real keys. Not listed: Boot, Menu,
// HardwareNotReady (SF-02 exits) and ShuttingDown (section 3.1.1 screen 10b: no keys work).
bool backReturnsToMenu(AppState state) noexcept {
    switch (state) {
        case AppState::Playing:          // #10
        case AppState::Paused:           // #10
        case AppState::GameOver:         // #10
        case AppState::HighScores:       // #12
        case AppState::Initials:         // #12
        case AppState::KeyboardLost:     // #13
        case AppState::ShutdownConfirm:  // #15
            return true;
        default:
            return false;
    }
}

bool backPressed(const InputFrame& input) noexcept {
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type == InputEventType::KeyDown && event.action == Action::Back) {
            return true;
        }
    }
    return false;
}

}  // namespace

App::App(EngineApi& api, const GameRegistryRow* rows, std::size_t rowCount, BestScoreFn bestScore)
    : api_(api) {
    std::vector<MenuGame> menuGames;
    games_.reserve(rowCount);
    menuGames.reserve(rowCount);
    for (std::size_t i = 0; i < rowCount; ++i) {
        games_.push_back(rows[i].factory());  // SR-FW-03, SR-FW-04
        menuGames.push_back(MenuGame{&rows[i], games_.back().get()});
    }
    menu_ = std::make_unique<MenuScreen>(std::move(menuGames), bestScore);
}

void App::setState(AppState state) noexcept {
    state_ = state;
    if (const auto context = contextFor(state, playContext_)) {
        api_.setInputContext(*context);  // SR-INP-09
    }
}

void App::bootFinished() noexcept {
    if (state_ == AppState::Boot) {
        setState(AppState::Menu);  // SR-FW-01
    }
}

Screen* App::screenFor(AppState state) noexcept {
    switch (state) {
        case AppState::Boot:
            return &boot_;
        case AppState::HardwareNotReady:
            return &hardwareNotReady_;
        case AppState::Menu:
            return menu_.get();
        case AppState::HighScores:
            return &highScores_;
        case AppState::ShutdownConfirm:
            return &shutdownConfirm_;
        case AppState::ShuttingDown:
            return &shuttingDown_;
        case AppState::Playing:
            return &playing_;
        case AppState::Paused:
            return &paused_;
        case AppState::KeyboardLost:
            return &keyboardLost_;
        case AppState::GameOver:
            return &gameOver_;
        case AppState::Initials:
            return &initials_;
    }
    return &boot_;
}

void App::update(const InputFrame& input) noexcept {
    if (backReturnsToMenu(state_) && backPressed(input)) {
        setState(AppState::Menu);  // SR-UI-07, SR-UI-12
        return;
    }
    screenFor(state_)->update(input);
    if (state_ != AppState::Menu) {
        return;
    }
    const MenuScreen::Request request = menu_->takeRequest();
    if (request == MenuScreen::Request::StartGame) {  // SR-UI-03: same tick
        const MenuGame* selected = menu_->selectedGame();
        // TBL-KeyBinding has contexts for the two release games only; others use Breakout's.
        playContext_ =
            selected->row->game_id == kGameIdSnake ? InputContext::Snake : InputContext::Breakout;
        playing_.start(*selected->game);
        setState(AppState::Playing);
    } else if (request == MenuScreen::Request::Scores) {
        setState(AppState::HighScores);
    }
}

void App::render(EngineApi& api) noexcept { screenFor(state_)->render(api); }

}  // namespace arcade
