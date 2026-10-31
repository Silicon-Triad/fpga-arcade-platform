#include "engine/hint_text.hpp"

#include <algorithm>

namespace arcade {
namespace {

constexpr std::string_view kSoundOn = "ON";
constexpr std::string_view kSoundOff = "OFF";

// Text before and after the sound state; `sound` is false where section 3.1.1 prints none.
struct HintRow {
    int row;
    std::string_view before;
    bool sound;
    std::string_view after;
};

// Section 3.1.1 screen layouts.
constexpr HintRow hintRow(HintScreen screen) noexcept {
    switch (screen) {
        case HintScreen::Menu:
            return {kHintRowOther, "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: ", true,
                    "  HOLD ESC: SHUT DOWN"};
        case HintScreen::Snake:
            return {kHintRowPlay, "ARROWS TURN  P PAUSE  ESC MENU  M SOUND: ", true, ""};
        case HintScreen::Breakout:
            return {kHintRowPlay, "</> MOVE  SPACE LAUNCH  P PAUSE  ESC MENU  M SOUND: ", true, ""};
        case HintScreen::Paused:
            return {kHintRowPlay, "P RESUME  ESC MENU  M SOUND: ", true, ""};
        case HintScreen::GameOver:  // the SRS gives no string; built from the screen's keys
            return {kHintRowPlay, "ENTER PLAY AGAIN  ESC MENU  M SOUND: ", true, ""};
        case HintScreen::Initials:  // M types a letter here (SR-INP-09)
            return {kHintRowOther, "A-Z TYPE  BACKSPACE DELETE  ENTER CONFIRM", false, ""};
        case HintScreen::HighScores:
            return {kHintRowOther, "</> SWITCH GAME  ESC BACK  M SOUND: ", true, ""};
        case HintScreen::NoKeyboard:  // empty because no keys work
            return {kHintRowPlay, "", false, ""};
        case HintScreen::ShutdownConfirm:
            return {kHintRowOther, "ENTER CONFIRM  ESC CANCEL", false, ""};
        case HintScreen::Boot:
        case HintScreen::SafeOff:
            break;
    }
    return {0, "", false, ""};
}

void append(HintBar& bar, std::string_view text) noexcept {
    std::copy(text.begin(), text.end(), bar.chars.begin() + static_cast<long>(bar.length));
    bar.length += text.size();
}

}  // namespace

std::optional<HintBar> hintBarFor(HintScreen screen, bool soundOn) noexcept {
    if (screen == HintScreen::Boot || screen == HintScreen::SafeOff) {
        return std::nullopt;
    }
    const HintRow row = hintRow(screen);
    HintBar bar;
    bar.row = row.row;
    append(bar, row.before);
    if (row.sound) {
        append(bar, soundOn ? kSoundOn : kSoundOff);  // SR-UI-13: current sound state
    }
    append(bar, row.after);
    return bar;
}

}  // namespace arcade
