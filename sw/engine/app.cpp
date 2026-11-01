#include "engine/app.hpp"

#include <optional>
#include <string_view>
#include <utility>

#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// SR-UI-15. DESIGN CHOICE, NOT AN SRS VALUE: the SRS names the notice but gives no layout. Row 90
// is free on the menu, initials and high-score screens, and only overlays the game-over picture.
constexpr int kNoticeRow = 90;
constexpr std::string_view kNoKeyboardNotice = "NO KEYBOARD - PLUG IN A USB KEYBOARD";

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
// HardwareNotReady (SF-02 exits), Initials (TBL-KeyBinding gives it no Back) and ShuttingDown
// (section 3.1.1 screen 10b: no keys work).
bool backReturnsToMenu(AppState state) noexcept {
    switch (state) {
        case AppState::HighScores:  // SR-UI-12
            return true;
        default:
            return false;
    }
}

// SR-UI-15, plus the shutdown box: the menu stays under it (section 3.1.1 screen 10a).
bool showsKeyboardNotice(AppState state) noexcept {
    switch (state) {
        case AppState::Menu:
        case AppState::HighScores:
        case AppState::ShutdownConfirm:
        case AppState::GameOver:
        case AppState::Initials:
            return true;
        default:
            return false;
    }
}

bool pressed(const InputFrame& input, Action action) noexcept {
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type == InputEventType::KeyDown && event.action == action) {
            return true;
        }
    }
    return false;
}

}  // namespace

App::App(EngineApi& api, const GameRegistryRow* rows, std::size_t rowCount, BestScoreFn bestScore,
         HighScoreStore* scores, std::function<void()> saveScores)
    : api_(api), scores_(scores), saveScores_(std::move(saveScores)) {
    playing_.setScores(scores);
    std::vector<MenuGame> menuGames;
    games_.reserve(rowCount);
    menuGames.reserve(rowCount);
    for (std::size_t i = 0; i < rowCount; ++i) {
        games_.push_back(rows[i].factory());  // SR-FW-03, SR-FW-04
        menuGames.push_back(MenuGame{&rows[i], games_.back().get()});
    }
    menu_ =
        std::make_unique<MenuScreen>(std::move(menuGames), std::move(bestScore), api_, soundOn_);
    highScores_.setSource(&menu_->games(), scores);
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

void App::setKeyboardMissing(bool missing) noexcept {
    keyboardMissing_ = missing;
    boot_.setKeyboardMissing(missing);  // section 3.1.1 screen 1
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

bool App::handleKeyboardEvents(const InputFrame& input) noexcept {
    bool finished = false;
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEventType type = input.events[i].type;
        if (type == InputEventType::KeyboardLost) {
            setKeyboardMissing(true);
            if (state_ == AppState::Playing || state_ == AppState::Paused) {
                setState(AppState::KeyboardLost);  // SR-UI-11: within 1 tick
            }
            finished = true;  // a key sent just before the unplug must not quit the game (SF-03)
        } else if (type == InputEventType::KeyboardFound) {
            setKeyboardMissing(false);
            if (state_ == AppState::KeyboardLost) {
                setState(AppState::Paused);  // SR-UI-11: resumes only on Pause
                finished = true;
            }
        }
    }
    return finished;
}

bool App::handleGameplayKeys(const InputFrame& input) noexcept {
    if (state_ != AppState::Playing && state_ != AppState::Paused && state_ != AppState::GameOver) {
        return false;
    }
    if (pressed(input, Action::Back)) {
        setState(AppState::Menu);  // SR-UI-07, SR-UI-09: nothing is saved
    } else if (state_ == AppState::Playing && pressed(input, Action::Pause)) {
        setState(AppState::Paused);  // SR-UI-05
    } else if (state_ == AppState::Paused && pressed(input, Action::Pause)) {
        setState(AppState::Playing);  // SR-UI-06
    } else if (state_ == AppState::GameOver && pressed(input, Action::Select)) {
        playing_.restart();                    // SR-UI-09
        api_.playSound(EffectId::MenuSelect);  // SR-GAM-16
        setState(AppState::Playing);
    } else {
        return false;
    }
    return true;
}

// SR-UI-10: the only place an entry is added, once per confirmed Initials screen. A failed save
// keeps the entry in memory and pending, so shutdown tries again (SR-FW-13).
void App::finishInitials() noexcept {
    const std::uint8_t gameId = playing_.gameId();
    std::uint8_t rank = HighScoresScreen::kNoHighlight;
    if (scores_ != nullptr && playing_.game() != nullptr) {
        rank = scores_->insert(gameId, initials_.letters(), playing_.game()->score())
                   .value_or(HighScoresScreen::kNoHighlight);
    }
    if (rank != HighScoresScreen::kNoHighlight && saveScores_) {
        saveScores_();  // SR-FW-09
    }
    highScores_.open(gameId, rank);
    setState(AppState::HighScores);
}

void App::handleMute(const InputFrame& input) noexcept {
    // Section 3.1.1: no keys work on screens without a context. On 10b a toggle would also undo
    // the shutdown mute (SR-FW-13). Initials needs no check: M arrives as Char (SR-INP-09).
    if (!contextFor(state_, playContext_)) {
        return;
    }
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type == InputEventType::KeyDown && event.action == Action::Mute) {
            soundOn_ = !soundOn_;  // SR-FW-11
            api_.setMute(!soundOn_);
        }
    }
}

void App::update(const InputFrame& input) noexcept {
    handleMute(input);  // first, so the Mute is not lost if a key below ends the tick
    if (handleKeyboardEvents(input)) {
        return;
    }
    if (backReturnsToMenu(state_) && pressed(input, Action::Back)) {
        setState(AppState::Menu);  // SR-UI-12
        return;
    }
    if (handleGameplayKeys(input)) {
        return;
    }
    screenFor(state_)->update(input);
    if (state_ == AppState::Initials && initials_.takeConfirmed()) {
        finishInitials();  // SR-UI-10
        return;
    }
    if (state_ == AppState::Playing && playing_.game() != nullptr && playing_.game()->isOver()) {
        // SR-UI-08
        if (playing_.scoreQualifies()) {
            initials_.begin(playing_.name(), playing_.game()->score(),
                            scores_->rankFor(playing_.gameId(), playing_.game()->score()));
            setState(AppState::Initials);
        } else {
            setState(AppState::GameOver);
        }
        return;
    }
    if (state_ == AppState::ShutdownConfirm) {
        const ShutdownConfirmScreen::Request answer = shutdownConfirm_.takeRequest();
        if (answer == ShutdownConfirmScreen::Request::Confirm) {
            setState(AppState::ShuttingDown);  // SR-UI-14
        } else if (answer == ShutdownConfirmScreen::Request::Cancel) {
            setState(AppState::Menu);  // SR-UI-14
        }
        return;
    }
    if (state_ != AppState::Menu) {
        return;
    }
    const MenuScreen::Request request = menu_->takeRequest();
    if (request == MenuScreen::Request::StartGame) {  // SR-UI-03: same tick
        const MenuGame* selected = menu_->selectedGame();
        // TBL-KeyBinding has contexts for the two release games only; others use Breakout's.
        const bool snake = selected->row->game_id == kGameIdSnake;
        playContext_ = snake ? InputContext::Snake : InputContext::Breakout;
        playing_.start(*selected->game, selected->row->game_id, selected->row->name,
                       snake ? HintScreen::Snake : HintScreen::Breakout);
        api_.playSound(EffectId::MenuSelect);  // SR-GAM-16
        setState(AppState::Playing);
    } else if (request == MenuScreen::Request::Scores) {
        highScores_.open();  // SR-UI-12
        setState(AppState::HighScores);
    } else if (request == MenuScreen::Request::Shutdown) {
        setState(AppState::ShutdownConfirm);  // SR-UI-14
    }
}

void App::render(EngineApi& api) noexcept {
    if (state_ == AppState::ShutdownConfirm) {
        menu_->render(api);  // section 3.1.1 screen 10a: the main menu stays in place
    }
    screenFor(state_)->render(api);
    if (keyboardMissing_ && showsKeyboardNotice(state_)) {
        drawCentred(api, kNoticeRow, kNoKeyboardNotice, TextScale::X1);  // SR-UI-15
    }
}

}  // namespace arcade
