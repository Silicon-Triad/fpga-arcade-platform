#include "engine/screens/playing_screen.hpp"

namespace arcade {

void PlayingScreen::start(Game& game) noexcept {
    game_ = &game;
    game_->init();
}

void PlayingScreen::update(const InputFrame& input) noexcept {
    if (game_ != nullptr) {
        game_->update(input);
    }
}

void PlayingScreen::render(EngineApi& api) noexcept {
    if (game_ != nullptr) {
        api.clear(0);  // tile 0 is blank (SR-REN-07)
        game_->render(api);
    }
}

}  // namespace arcade
