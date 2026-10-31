#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <optional>
#include <string_view>

#include "arcade/default_keymap.hpp"
#include "arcade/tables.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Why a keymap.conf line was not applied (SC-05). Diagnostic only: every such line is ignored
/// and the default binding for that key stays.
enum class KeymapLineError : std::uint8_t {
    WrongFieldCount,  ///< Not exactly three fields: <Context> <KEY_NAME> <Action>.
    UnknownContext,   ///< Not one of the TBL-KeyBinding context names.
    UnknownKey,       ///< Not a KEY_* name that has a default binding.
    UnknownAction,    ///< Not one of the TBL-KeyBinding action names.
    NoDefaultRow,     ///< The key has no default row in that context; overrides only replace.
    CharNotLetter,    ///< Char is allowed only for KEY_A to KEY_Z in Initials (SR-INP-11).
};

/// Result of loading keymap.conf (SC-05).
struct KeymapLoadResult {
    std::size_t applied = 0;  ///< Lines that replaced a row.
    std::size_t ignored = 0;  ///< Non-blank, non-comment lines that were ignored.
    /// First ignored line (1-based) and why, for a start-up log message; 0 if none.
    std::size_t firstIgnoredLine = 0;
    KeymapLineError firstError = KeymapLineError::WrongFieldCount;
};

/// TBL-KeyBinding at run time: the compiled-in default rows, with individual rows replaced by
/// /etc/arcade/keymap.conf (SR-INP-09 to SR-INP-11, SC-05). Fixed size: an override can only
/// replace an existing (context, key_code) row, never add one, so lookups need no allocation.
class KeyBindings {
  public:
    /// SC-05: the optional override file, read once at start-up.
    static constexpr std::string_view kKeymapFile = "/etc/arcade/keymap.conf";

    /// Starts with the Chapter 5 default rows (kDefaultKeyBindings).
    KeyBindings();

    /// SR-INP-09, SR-INP-10: the action bound to `keyCode` in `context`, or else in Global;
    /// std::nullopt if neither has a row (the key is unbound and its event is dropped).
    std::optional<Action> lookup(InputContext context, std::uint16_t keyCode) const;

    /// SR-INP-09 to SR-INP-11: turns a KeyDown of `keyCode` into an action event. Returns
    /// std::nullopt for an unbound key (SR-INP-10). A Char action carries the letter 'A'-'Z' of
    /// the key in InputEvent::ch (SR-INP-11).
    std::optional<InputEvent> translateKeyDown(InputContext context, std::uint16_t keyCode) const;

    /// SC-05: applies keymap.conf lines from `in`. Each line is `<Context> <KEY_NAME> <Action>`
    /// separated by spaces or tabs; `#` starts a comment and blank lines are skipped. A line
    /// that names an unknown context, key or action, or a key with no default row in that
    /// context, is ignored and the existing binding stays. Later lines win. Start-up only.
    KeymapLoadResult loadOverrides(std::istream& in);

    /// SC-05: loadOverrides() on kKeymapFile; a missing or unreadable file keeps the defaults.
    KeymapLoadResult loadOverrideFile();

    /// The current rows, in default-table order.
    const std::array<KeyBindingRow, kDefaultKeyBindingCount>& rows() const { return rows_; }

    /// SR-INP-11: the letter 'A'-'Z' of `keyCode` if it is KEY_A to KEY_Z, else 0.
    static char letterOf(std::uint16_t keyCode);

  private:
    std::optional<KeymapLineError> applyLine(std::string_view line, bool& blank);

    std::array<KeyBindingRow, kDefaultKeyBindingCount> rows_;
};

}  // namespace arcade
