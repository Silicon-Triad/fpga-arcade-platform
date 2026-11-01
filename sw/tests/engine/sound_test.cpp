// Unit tests for sound on/off and the menu sound events (issue #14, SR-FW-11, SR-UI-13,
// SR-GAM-15, SR-GAM-16).
#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "engine/app.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

constexpr int kHintRow = 93;       // section 3.1.1 screen 2
constexpr int kStatusRow = 86;     // section 3.1.1 screen 2
constexpr int kHintCol = 2;        // ui_layout.cpp
constexpr int kStatusCol = 13;     // section 3.1.1 screen 2, same column as the prompt
constexpr char kFirstGlyph = ' ';  // font tile 0

class SilentGame final : public Game {
  public:
    void init() noexcept override {}
    void update(const InputFrame&) noexcept override {}
    void render(ArcadeApi&) noexcept override {}
    bool isOver() const noexcept override { return false; }
    std::uint32_t score() const noexcept override { return 0; }
};

std::unique_ptr<Game> makeGame() { return std::make_unique<SilentGame>(); }

constexpr std::array<GameRegistryRow, 2> kRows{{
    {kGameIdSnake, "SNAKE", 0, &makeGame},
    {kGameIdBreakout, "BREAKOUT", 1, &makeGame},
}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame press(Action action) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    frame.count = 1;
    return frame;
}

class SoundTest : public ::testing::Test {
  protected:
    SoundTest() : app(api, kRows.data(), kRows.size(), &noScore) { app.setState(AppState::Menu); }

    // Golden tilemap: reads `length` font tiles back as text.
    std::string textAt(int col, int row, std::size_t length) const {
        std::string text;
        for (std::size_t i = 0; i < length; ++i) {
            const auto x = static_cast<std::size_t>(col) + i;
            text += static_cast<char>(api.cells[static_cast<std::size_t>(row)][x] + kFirstGlyph);
        }
        return text;
    }

    FakeEngineApi api;
    App app;
};

TEST_F(SoundTest, SR_FW_11_OnAtStartUpWithNoMuteCall) {
    EXPECT_TRUE(app.soundOn());
    EXPECT_TRUE(api.mutes.empty());
}

TEST_F(SoundTest, SR_FW_11_EachMuteTogglesWithOneSetMuteCall) {
    app.update(press(Action::Mute));
    EXPECT_FALSE(app.soundOn());
    EXPECT_EQ(api.mutes, std::vector<bool>({true}));
    app.update(press(Action::Mute));
    EXPECT_TRUE(app.soundOn());
    EXPECT_EQ(api.mutes, std::vector<bool>({true, false}));
}

TEST_F(SoundTest, SR_FW_11_KeyUpDoesNotToggle) {
    InputFrame frame = press(Action::Mute);
    frame.events[0].type = InputEventType::KeyUp;
    app.update(frame);
    EXPECT_TRUE(app.soundOn());
    EXPECT_TRUE(api.mutes.empty());
}

TEST_F(SoundTest, SR_FW_11_TwoMutesInOneTickToggleTwice) {
    InputFrame frame = press(Action::Mute);
    frame.events[1] = frame.events[0];
    frame.count = 2;
    app.update(frame);
    EXPECT_TRUE(app.soundOn());
    EXPECT_EQ(api.mutes, std::vector<bool>({true, false}));
}

TEST_F(SoundTest, SR_FW_11_MuteIsNotLostWhenBackEndsTheTick) {
    app.setState(AppState::HighScores);
    InputFrame frame = press(Action::Back);
    frame.events[1] = InputEvent{InputEventType::KeyDown, Action::Mute, 0};
    frame.count = 2;
    app.update(frame);
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_FALSE(app.soundOn());
}

TEST_F(SoundTest, SR_FW_11_SettingSurvivesScreenChanges) {
    app.update(press(Action::Mute));
    app.update(press(Action::Select));  // start a game
    app.update(press(Action::Back));
    EXPECT_FALSE(app.soundOn());
    EXPECT_EQ(api.mutes.size(), 1U);
}

class MuteOn : public ::testing::TestWithParam<AppState> {};

TEST_P(MuteOn, SR_FW_11_TogglesOnEveryScreenWithKeys) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(GetParam());
    app.update(press(Action::Mute));
    EXPECT_FALSE(app.soundOn());
    EXPECT_EQ(app.state(), GetParam());
}

INSTANTIATE_TEST_SUITE_P(Screens, MuteOn,
                         ::testing::Values(AppState::Menu, AppState::Playing, AppState::Paused,
                                           AppState::GameOver, AppState::HighScores,
                                           AppState::ShutdownConfirm));

class MuteIgnoredOn : public ::testing::TestWithParam<AppState> {};

// Section 3.1.1: no keys work on these screens; on 10b a toggle would undo SR-FW-13's mute.
TEST_P(MuteIgnoredOn, Sec311_NoKeysWork) {
    FakeEngineApi api;
    App app(api, kRows.data(), kRows.size(), &noScore);
    app.setState(GetParam());
    app.update(press(Action::Mute));
    EXPECT_TRUE(app.soundOn());
    EXPECT_TRUE(api.mutes.empty());
}

INSTANTIATE_TEST_SUITE_P(Screens, MuteIgnoredOn,
                         ::testing::Values(AppState::Boot, AppState::HardwareNotReady,
                                           AppState::KeyboardLost, AppState::ShuttingDown));

TEST_F(SoundTest, SR_INP_09_MOnInitialsTypesALetterAndDoesNotMute) {
    app.setState(AppState::Initials);
    EXPECT_EQ(api.contexts.back(), InputContext::Initials);
    InputFrame frame = press(Action::Char);
    frame.events[0].ch = 'M';
    app.update(frame);
    EXPECT_TRUE(app.soundOn());
    EXPECT_TRUE(api.mutes.empty());
}

TEST_F(SoundTest, SR_UI_13_HintBarAndStatusLineFollowEachToggle) {
    const std::string hintOn =
        "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: ON  HOLD ESC: SHUT DOWN";
    const std::string hintOff =
        "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: OFF  HOLD ESC: SHUT DOWN";
    const std::string statusOn = "SOUND ON - 2 GAMES - TOP 10 SAVED";
    const std::string statusOff = "SOUND OFF - 2 GAMES - TOP 10 SAVED";

    app.render(api);
    EXPECT_EQ(textAt(kHintCol, kHintRow, hintOn.size()), hintOn);
    EXPECT_EQ(textAt(kStatusCol, kStatusRow, statusOn.size()), statusOn);

    app.update(press(Action::Mute));  // same tick: update, then render
    app.render(api);
    EXPECT_EQ(textAt(kHintCol, kHintRow, hintOff.size()), hintOff);
    EXPECT_EQ(textAt(kStatusCol, kStatusRow, statusOff.size()), statusOff);

    app.update(press(Action::Mute));
    app.render(api);
    EXPECT_EQ(textAt(kHintCol, kHintRow, hintOn.size() + 1), hintOn + " ");
    EXPECT_EQ(textAt(kStatusCol, kStatusRow, statusOn.size() + 1), statusOn + " ");
}

TEST_F(SoundTest, SR_UI_13_PlayingScreenSeesTheEngineState) {
    EXPECT_TRUE(app.playing().soundOn());
    app.update(press(Action::Mute));
    EXPECT_FALSE(app.playing().soundOn());
}

TEST_F(SoundTest, SR_GAM_15_UpAndDownEachPlayOneMenuMove) {
    app.update(press(Action::Down));
    EXPECT_EQ(api.effects, std::vector<EffectId>({EffectId::MenuMove}));
    app.update(press(Action::Up));
    EXPECT_EQ(api.effects, std::vector<EffectId>({EffectId::MenuMove, EffectId::MenuMove}));
}

TEST_F(SoundTest, SR_GAM_16_SelectOnGamePlaysOneMenuSelect) {
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Playing);
    EXPECT_EQ(api.effects, std::vector<EffectId>({EffectId::MenuSelect}));
}

TEST_F(SoundTest, SR_GAM_16_SelectOnScoresPlaysNothing) {
    app.update(press(Action::Up));  // wraps to SCORES
    api.effects.clear();
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::HighScores);
    EXPECT_TRUE(api.effects.empty());
}

TEST_F(SoundTest, SR_FW_07_MutedEventsStillCallPlaySound) {
    app.update(press(Action::Mute));
    app.update(press(Action::Down));
    app.update(press(Action::Up));
    app.update(press(Action::Select));
    EXPECT_EQ(api.effects, std::vector<EffectId>(
                               {EffectId::MenuMove, EffectId::MenuMove, EffectId::MenuSelect}));
}

}  // namespace
}  // namespace arcade
