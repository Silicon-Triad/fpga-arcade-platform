// Unit tests for TBL-KeyBinding: the default rows, the lookup and the keymap.conf override
// (issue #21, SR-INP-09 to SR-INP-11, SC-05).
#include "input/key_bindings.hpp"

#include <gtest/gtest.h>
#include <linux/input.h>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace arcade {
namespace {

using Binding = std::pair<std::uint16_t, Action>;

/// Chapter 5 "Default bindings", typed again here independently of default_keymap.hpp, for
/// every context except Initials (its 26 letters are checked in their own test).
void expectBindings(const KeyBindings& kb, InputContext ctx, const std::vector<Binding>& want) {
    for (const auto& [key, action] : want) {
        EXPECT_EQ(kb.lookup(ctx, key), std::optional<Action>(action)) << "key " << key;
    }
}

/// The translated KeyDown, or a KeyUp sentinel if there is none, so a missing event fails the
/// checks that follow without dereferencing an empty optional.
InputEvent down(const KeyBindings& kb, InputContext ctx, std::uint16_t key) {
    InputEvent none;
    none.type = InputEventType::KeyUp;
    return kb.translateKeyDown(ctx, key).value_or(none);
}

bool sameRows(const KeyBindings& a, const KeyBindings& b) {
    for (std::size_t i = 0; i < a.rows().size(); ++i) {
        const KeyBindingRow& x = a.rows()[i];
        const KeyBindingRow& y = b.rows()[i];
        if (x.context != y.context || x.key_code != y.key_code || x.action != y.action) {
            return false;
        }
    }
    return true;
}

std::size_t rowsIn(InputContext ctx) {
    std::size_t n = 0;
    for (const KeyBindingRow& row : kDefaultKeyBindings) {
        n += (row.context == ctx) ? 1U : 0U;
    }
    return n;
}

// --- The default table (Chapter 5) ------------------------------------------------------------

TEST(DefaultKeymap, TBL_KeyBinding_RowCountsPerContext) {
    EXPECT_EQ(kDefaultKeyBindings.size(), 54U);
    EXPECT_EQ(rowsIn(InputContext::Global), 1U);
    EXPECT_EQ(rowsIn(InputContext::Menu), 5U);
    EXPECT_EQ(rowsIn(InputContext::Snake), 6U);
    EXPECT_EQ(rowsIn(InputContext::Breakout), 5U);
    EXPECT_EQ(rowsIn(InputContext::Paused), 2U);
    EXPECT_EQ(rowsIn(InputContext::GameOver), 2U);
    EXPECT_EQ(rowsIn(InputContext::Initials), 28U);
    EXPECT_EQ(rowsIn(InputContext::HighScores), 3U);
    EXPECT_EQ(rowsIn(InputContext::ShutdownConfirm), 2U);
}

TEST(DefaultKeymap, TBL_KeyBinding_PrimaryKeyIsUnique) {
    std::set<std::pair<InputContext, std::uint16_t>> seen;
    for (const KeyBindingRow& row : kDefaultKeyBindings) {
        EXPECT_TRUE(seen.insert({row.context, row.key_code}).second)
            << "duplicate key " << row.key_code;
    }
}

// --- SR-INP-09: one test per context ----------------------------------------------------------

TEST(KeyBindings, SR_INP_09_Global) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Global, {{KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_Menu) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Menu,
                   {{KEY_UP, Action::Up},
                    {KEY_DOWN, Action::Down},
                    {KEY_ENTER, Action::Select},
                    {KEY_H, Action::Select},
                    {KEY_ESC, Action::Back},
                    {KEY_M, Action::Mute}});  // Global fallback
}

TEST(KeyBindings, SR_INP_09_Snake) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Snake,
                   {{KEY_UP, Action::Up},
                    {KEY_DOWN, Action::Down},
                    {KEY_LEFT, Action::Left},
                    {KEY_RIGHT, Action::Right},
                    {KEY_P, Action::Pause},
                    {KEY_ESC, Action::Back}});
}

TEST(KeyBindings, SR_INP_09_MInSnakeGivesMute) {
    const KeyBindings kb;
    const InputEvent ev = down(kb, InputContext::Snake, KEY_M);
    EXPECT_EQ(ev.type, InputEventType::KeyDown);
    EXPECT_EQ(ev.action, Action::Mute);
    EXPECT_EQ(ev.ch, 0);
}

TEST(KeyBindings, SR_INP_09_Breakout) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Breakout,
                   {{KEY_LEFT, Action::Left},
                    {KEY_RIGHT, Action::Right},
                    {KEY_SPACE, Action::Launch},
                    {KEY_P, Action::Pause},
                    {KEY_ESC, Action::Back},
                    {KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_Paused) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Paused,
                   {{KEY_P, Action::Pause}, {KEY_ESC, Action::Back}, {KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_GameOver) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::GameOver,
                   {{KEY_ENTER, Action::Select}, {KEY_ESC, Action::Back}, {KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_HighScores) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::HighScores,
                   {{KEY_LEFT, Action::Left},
                    {KEY_RIGHT, Action::Right},
                    {KEY_ESC, Action::Back},
                    {KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_ShutdownConfirm) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::ShutdownConfirm,
                   {{KEY_ENTER, Action::Select}, {KEY_ESC, Action::Back}, {KEY_M, Action::Mute}});
}

TEST(KeyBindings, SR_INP_09_InitialsBackspaceAndEnter) {
    const KeyBindings kb;
    expectBindings(kb, InputContext::Initials,
                   {{KEY_BACKSPACE, Action::Delete}, {KEY_ENTER, Action::Select}});
}

// --- SR-INP-11: Initials letters ---------------------------------------------------------------

TEST(KeyBindings, SR_INP_11_MInInitialsGivesCharM) {
    const KeyBindings kb;
    const InputEvent ev = down(kb, InputContext::Initials, KEY_M);
    EXPECT_EQ(ev.type, InputEventType::KeyDown);
    EXPECT_EQ(ev.action, Action::Char);  // overrides Global M -> Mute (SR-INP-09)
    EXPECT_EQ(ev.ch, 'M');
}

TEST(KeyBindings, SR_INP_11_All26LettersMap) {
    const std::array<std::uint16_t, 26> letters{
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};
    const KeyBindings kb;
    for (std::size_t i = 0; i < letters.size(); ++i) {
        const char want = static_cast<char>('A' + static_cast<int>(i));
        const InputEvent ev = down(kb, InputContext::Initials, letters[i]);
        EXPECT_EQ(ev.type, InputEventType::KeyDown) << want;
        EXPECT_EQ(ev.action, Action::Char) << want;
        EXPECT_EQ(ev.ch, want);
    }
}

TEST(KeyBindings, SR_INP_11_LetterOfNonLetterIsZero) {
    EXPECT_EQ(KeyBindings::letterOf(KEY_ENTER), 0);
    EXPECT_EQ(KeyBindings::letterOf(KEY_1), 0);
    EXPECT_EQ(KeyBindings::letterOf(KEY_Q), 'Q');
}

// --- SR-INP-10: unbound keys are dropped -------------------------------------------------------

TEST(KeyBindings, SR_INP_10_UnboundKeysAreDropped) {
    const KeyBindings kb;
    EXPECT_FALSE(kb.translateKeyDown(InputContext::Snake, KEY_SPACE).has_value());
    EXPECT_FALSE(kb.translateKeyDown(InputContext::Breakout, KEY_UP).has_value());
    EXPECT_FALSE(kb.translateKeyDown(InputContext::Menu, KEY_A).has_value());
    EXPECT_FALSE(kb.translateKeyDown(InputContext::Initials, KEY_1).has_value());
    EXPECT_FALSE(kb.translateKeyDown(InputContext::Paused, KEY_F1).has_value());
}

// --- SC-05: keymap.conf --------------------------------------------------------------------------

KeymapLoadResult load(KeyBindings& kb, const std::string& text) {
    std::istringstream in(text);
    return kb.loadOverrides(in);
}

TEST(Keymap, SC_05_ValidLinesReplaceRows) {
    KeyBindings kb;
    const auto r = load(kb,
                        "# swap Snake Up and Down\n"
                        "Snake KEY_UP Down\n"
                        "\tSnake\tKEY_DOWN\tUp   # tabs and a trailing comment\n"
                        "\n"
                        "Global KEY_M Pause\n");
    EXPECT_EQ(r.applied, 3U);
    EXPECT_EQ(r.ignored, 0U);
    EXPECT_EQ(kb.lookup(InputContext::Snake, KEY_UP), Action::Down);
    EXPECT_EQ(kb.lookup(InputContext::Snake, KEY_DOWN), Action::Up);
    EXPECT_EQ(kb.lookup(InputContext::Breakout, KEY_M), Action::Pause);  // through Global
}

TEST(Keymap, SC_05_InvalidLinesAreIgnoredAndDefaultsStay) {
    KeyBindings kb;
    const auto r = load(kb,
                        "Pong KEY_UP Up\n"            // unknown context
                        "Snake KEY_FOO Up\n"          // unknown key
                        "Snake KEY_UP Jump\n"         // unknown action
                        "Snake KEY_W Up\n"            // no default row: replace-only
                        "Snake KEY_UP\n"              // too few fields
                        "Snake KEY_UP Up extra\n"     // too many fields
                        "snake KEY_UP Down\n"         // names are case-sensitive
                        "Snake KEY_P Char\n"          // Char outside Initials
                        "Initials KEY_ENTER Char\n"   // Char on a non-letter
                        "\x01\x02\xff junk \x7f\n");  // binary junk
    EXPECT_EQ(r.applied, 0U);
    EXPECT_EQ(r.ignored, 10U);
    EXPECT_EQ(r.firstIgnoredLine, 1U);
    EXPECT_EQ(r.firstError, KeymapLineError::UnknownContext);
    // Every default binding still works (SC-05 verification).
    EXPECT_TRUE(sameRows(kb, KeyBindings()));
}

TEST(Keymap, SC_05_EachErrorIsReported) {
    const std::vector<std::pair<std::string, KeymapLineError>> cases{
        {"Pong KEY_UP Up", KeymapLineError::UnknownContext},
        {"Snake KEY_FOO Up", KeymapLineError::UnknownKey},
        {"Snake KEY_UP Jump", KeymapLineError::UnknownAction},
        {"Snake KEY_W Up", KeymapLineError::NoDefaultRow},
        {"Snake KEY_UP", KeymapLineError::WrongFieldCount},
        {"Snake KEY_P Char", KeymapLineError::CharNotLetter},
    };
    for (const auto& [line, error] : cases) {
        KeyBindings kb;
        const auto r = load(kb, line + "\n");
        EXPECT_EQ(r.ignored, 1U) << line;
        EXPECT_EQ(r.firstError, error) << line;
    }
}

TEST(Keymap, SC_05_GoodLinesApplyAroundBadOnes) {
    KeyBindings kb;
    const auto r = load(kb,
                        "Snake KEY_UP Jump\n"
                        "Breakout KEY_SPACE Pause\n"
                        "Breakout KEY_Q Launch\n");
    EXPECT_EQ(r.applied, 1U);
    EXPECT_EQ(r.ignored, 2U);
    EXPECT_EQ(r.firstIgnoredLine, 1U);
    EXPECT_EQ(kb.lookup(InputContext::Breakout, KEY_SPACE), Action::Pause);
    EXPECT_EQ(kb.lookup(InputContext::Snake, KEY_UP), Action::Up);  // default kept
    EXPECT_FALSE(kb.lookup(InputContext::Breakout, KEY_Q).has_value());
}

TEST(Keymap, SC_05_LaterLineWins) {
    KeyBindings kb;
    load(kb, "Menu KEY_H Back\nMenu KEY_H Up\n");
    EXPECT_EQ(kb.lookup(InputContext::Menu, KEY_H), Action::Up);
}

TEST(Keymap, SC_05_InitialsLetterCanBeRebound) {
    KeyBindings kb;
    load(kb, "Initials KEY_M Mute\n");
    const InputEvent ev = down(kb, InputContext::Initials, KEY_M);
    EXPECT_EQ(ev.type, InputEventType::KeyDown);
    EXPECT_EQ(ev.action, Action::Mute);
    EXPECT_EQ(ev.ch, 0);
}

TEST(Keymap, SC_05_EmptyOrCommentOnlyFileKeepsDefaults) {
    KeyBindings kb;
    const auto r = load(kb, "\n   \n# only comments\n\t# here too\n");
    EXPECT_EQ(r.applied, 0U);
    EXPECT_EQ(r.ignored, 0U);
    EXPECT_EQ(r.firstIgnoredLine, 0U);
    EXPECT_TRUE(sameRows(kb, KeyBindings()));
}

}  // namespace
}  // namespace arcade
