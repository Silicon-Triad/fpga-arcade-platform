// TBL-KeyBinding lookup and keymap.conf override (SR-INP-09 to SR-INP-11, SC-05).
#include "input/key_bindings.hpp"

#include <linux/input.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <utility>

namespace arcade {
namespace {

/// TBL-KeyBinding context names, as written in Chapter 5 and in keymap.conf.
constexpr std::array<std::pair<std::string_view, InputContext>, kInputContextCount> kContexts{{
    {"Global", InputContext::Global},
    {"Menu", InputContext::Menu},
    {"Snake", InputContext::Snake},
    {"Breakout", InputContext::Breakout},
    {"Paused", InputContext::Paused},
    {"GameOver", InputContext::GameOver},
    {"Initials", InputContext::Initials},
    {"HighScores", InputContext::HighScores},
    {"ShutdownConfirm", InputContext::ShutdownConfirm},
}};

/// TBL-KeyBinding action names, as written in Chapter 5 and in keymap.conf.
constexpr std::array<std::pair<std::string_view, Action>, kActionCount> kActions{{
    {"Up", Action::Up},
    {"Down", Action::Down},
    {"Left", Action::Left},
    {"Right", Action::Right},
    {"Select", Action::Select},
    {"Back", Action::Back},
    {"Pause", Action::Pause},
    {"Launch", Action::Launch},
    {"Char", Action::Char},
    {"Delete", Action::Delete},
    {"Mute", Action::Mute},
}};

/// SR-INP-11: KEY_A to KEY_Z in alphabet order. Linux key codes follow the keyboard layout,
/// not the alphabet, so the letter needs a table.
constexpr std::array<std::uint16_t, 26> kLetterKeys{
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
    KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};

/// KEY_* names accepted in keymap.conf: every key that has a default row (SC-05, replace-only;
/// any other key could never match a row). Letters are handled through kLetterKeys.
constexpr std::array<std::pair<std::string_view, std::uint16_t>, 8> kOtherKeys{{
    {"KEY_UP", KEY_UP},
    {"KEY_DOWN", KEY_DOWN},
    {"KEY_LEFT", KEY_LEFT},
    {"KEY_RIGHT", KEY_RIGHT},
    {"KEY_ENTER", KEY_ENTER},
    {"KEY_ESC", KEY_ESC},
    {"KEY_SPACE", KEY_SPACE},
    {"KEY_BACKSPACE", KEY_BACKSPACE},
}};

template <typename Table>
auto findByName(const Table& table,
                std::string_view name) -> std::optional<typename Table::value_type::second_type> {
    for (const auto& entry : table) {
        if (entry.first == name) {
            return entry.second;
        }
    }
    return std::nullopt;
}

std::optional<std::uint16_t> keyByName(std::string_view name) {
    constexpr std::string_view kPrefix = "KEY_";
    if (name.size() == kPrefix.size() + 1 && name.substr(0, kPrefix.size()) == kPrefix) {
        const char c = name.back();
        if (c >= 'A' && c <= 'Z') {
            return kLetterKeys[static_cast<std::size_t>(c - 'A')];
        }
    }
    return findByName(kOtherKeys, name);
}

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r'; }

}  // namespace

KeyBindings::KeyBindings() : rows_(kDefaultKeyBindings) {}

std::optional<Action> KeyBindings::lookup(InputContext context, std::uint16_t keyCode) const {
    // SR-INP-09: the current context first, so a screen can override a Global key, then Global.
    const std::array<InputContext, 2> order{context, InputContext::Global};
    for (const InputContext ctx : order) {
        for (const KeyBindingRow& row : rows_) {
            if (row.context == ctx && row.key_code == keyCode) {
                return row.action;
            }
        }
    }
    return std::nullopt;  // SR-INP-10: unbound in both
}

std::optional<InputEvent> KeyBindings::translateKeyDown(InputContext context,
                                                        std::uint16_t keyCode) const {
    const std::optional<Action> action = lookup(context, keyCode);
    if (!action) {
        return std::nullopt;  // SR-INP-10: the KeyDown is discarded
    }
    InputEvent ev;
    ev.type = InputEventType::KeyDown;
    ev.action = *action;
    if (*action == Action::Char) {
        ev.ch = letterOf(keyCode);  // SR-INP-11: Char carries the letter A-Z
    }
    return ev;
}

char KeyBindings::letterOf(std::uint16_t keyCode) {
    for (std::size_t i = 0; i < kLetterKeys.size(); ++i) {
        if (kLetterKeys[i] == keyCode) {
            return static_cast<char>('A' + static_cast<int>(i));
        }
    }
    return 0;
}

std::optional<KeymapLineError> KeyBindings::applyLine(std::string_view line, bool& blank) {
    // SC-05: `#` starts a comment; fields are separated by spaces or tabs.
    line = line.substr(0, line.find('#'));
    std::array<std::string_view, 3> fields{};
    std::size_t count = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && isSpace(line[i])) {
            ++i;
        }
        const std::size_t start = i;
        while (i < line.size() && !isSpace(line[i])) {
            ++i;
        }
        if (i > start) {
            if (count == fields.size()) {
                return KeymapLineError::WrongFieldCount;
            }
            fields[count++] = line.substr(start, i - start);
        }
    }
    blank = (count == 0);
    if (blank) {
        return std::nullopt;
    }
    if (count != fields.size()) {
        return KeymapLineError::WrongFieldCount;
    }
    const auto context = findByName(kContexts, fields[0]);
    if (!context) {
        return KeymapLineError::UnknownContext;
    }
    const auto key = keyByName(fields[1]);
    if (!key) {
        return KeymapLineError::UnknownKey;
    }
    const auto action = findByName(kActions, fields[2]);
    if (!action) {
        return KeymapLineError::UnknownAction;
    }
    // SR-INP-11: a Char event needs a letter, and Char is the Initials action.
    if (*action == Action::Char && (*context != InputContext::Initials || letterOf(*key) == 0)) {
        return KeymapLineError::CharNotLetter;
    }
    // SC-05, Chapter 5: the file may replace individual rows, not add new ones.
    const auto row = std::find_if(rows_.begin(), rows_.end(), [&](const KeyBindingRow& r) {
        return r.context == *context && r.key_code == *key;
    });
    if (row == rows_.end()) {
        return KeymapLineError::NoDefaultRow;
    }
    row->action = *action;
    return std::nullopt;
}

KeymapLoadResult KeyBindings::loadOverrides(std::istream& in) {
    KeymapLoadResult result;
    std::string line;
    std::size_t lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        bool blank = false;
        const std::optional<KeymapLineError> error = applyLine(line, blank);
        if (blank) {
            continue;
        }
        if (!error) {
            ++result.applied;
            continue;
        }
        // SC-05: ignore the line; the binding for that key stays as it was.
        if (result.ignored == 0) {
            result.firstIgnoredLine = lineNo;
            result.firstError = *error;
        }
        ++result.ignored;
    }
    return result;
}

KeymapLoadResult KeyBindings::loadOverrideFile() {
    std::ifstream in{std::string(kKeymapFile)};
    if (!in) {
        return {};  // SC-05: the file is optional; the defaults stay
    }
    return loadOverrides(in);
}

}  // namespace arcade
