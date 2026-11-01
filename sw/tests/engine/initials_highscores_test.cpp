// Unit tests for Initials entry and the High Scores screen (issue #12, SR-UI-10, SR-UI-12).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "assets/game_tiles.hpp"
#include "assets/tile_ids.hpp"
#include "engine/app.hpp"
#include "engine/ui_layout.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

class EndingGame final : public Game {
  public:
    void init() noexcept override { over = false; }
    void update(const InputFrame&) noexcept override {}
    void render(ArcadeApi&) noexcept override {}
    bool isOver() const noexcept override { return over; }
    std::uint32_t score() const noexcept override { return points; }
    bool over = false;
    std::uint32_t points = 560;
};

EndingGame* lastGame = nullptr;
std::unique_ptr<Game> makeGame() {
    auto game = std::make_unique<EndingGame>();
    lastGame = game.get();
    return game;
}

// Breakout is listed first on purpose: the selector follows menu_order, not game_id.
constexpr std::array<GameRegistryRow, 2> kRows{
    {{kGameIdBreakout, "BREAKOUT", 1, &makeGame}, {kGameIdSnake, "SNAKE", 0, &makeGame}}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame press(Action action, char ch = 0) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, ch};
    frame.count = 1;
    return frame;
}

bool drew(const FakeEngineApi& api, const std::string& entry) {
    return std::find(api.placed.begin(), api.placed.end(), entry) != api.placed.end();
}

// The parts screens 7 and 8 share: blank grid, frame, hint.
void goldenBase(FakeEngineApi& golden, std::string_view hint) {
    golden.clear(0);
    drawFrame(golden);
    golden.fillRect(1, 93, 126, 1, 0);
    golden.drawText(2, 93, hint, TextScale::X1);
}

class ScoresFlowTest : public ::testing::Test {
  protected:
    ScoresFlowTest()
        : app(api, kRows.data(), kRows.size(), &noScore, &scores, [this]() noexcept { ++saves; }) {
        snake = lastGame;  // the factories ran in registry order; Snake's row is last
        app.setState(AppState::Menu);
    }
    // Plays Snake (first in the menu) to the end with `points`.
    void finishGame(std::uint32_t points) {
        app.update(press(Action::Select));
        ASSERT_EQ(app.state(), AppState::Playing);
        snake->points = points;
        snake->over = true;
        app.update(InputFrame{});
    }
    void type(std::string_view letters) {
        for (const char letter : letters) {
            app.update(press(Action::Char, letter));
        }
    }
    void openFromMenu() {
        app.update(press(Action::Up));  // wraps to SCORES
        app.update(press(Action::Select));
        ASSERT_EQ(app.state(), AppState::HighScores);
    }
    void addSnake(std::uint32_t score) {
        ASSERT_TRUE(scores.insert(kGameIdSnake, {'O', 'L', 'D'}, score).has_value());
    }

    FakeEngineApi api;
    HighScoreStore scores;
    int saves = 0;
    App app;
    EndingGame* snake;
};

TEST_F(ScoresFlowTest, SR_UI_10_SelectRefusedBelowThreeLetters) {
    finishGame(560);
    ASSERT_EQ(app.state(), AppState::Initials);
    for (const std::string_view letters : {"", "A", "B"}) {
        type(letters);
        app.update(press(Action::Select));
        EXPECT_EQ(app.state(), AppState::Initials);
    }
    EXPECT_EQ(saves, 0);
    EXPECT_EQ(scores.table(kGameIdSnake).count, 0U);
}

TEST_F(ScoresFlowTest, SR_UI_10_FourthLetterAndNonLettersAreIgnored) {
    finishGame(560);
    type("ABCD");
    app.update(press(Action::Char, '1'));
    app.update(press(Action::Select));
    ASSERT_EQ(scores.table(kGameIdSnake).count, 1U);
    const auto& initials = scores.table(kGameIdSnake).rows[0].initials;
    EXPECT_EQ(std::string(initials.data(), initials.size()), "ABC");
}

TEST_F(ScoresFlowTest, SR_UI_10_DeleteRemovesLastLetter) {
    finishGame(560);
    app.update(press(Action::Delete));  // nothing to delete
    type("ABC");
    app.update(press(Action::Delete));
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Initials);
    type("Z");
    app.update(press(Action::Select));
    ASSERT_EQ(app.state(), AppState::HighScores);
    const auto& initials = scores.table(kGameIdSnake).rows[0].initials;
    EXPECT_EQ(std::string(initials.data(), initials.size()), "ABZ");
}

TEST_F(ScoresFlowTest, SR_UI_10_BackDoesNotLeaveInitials) {  // TBL-KeyBinding: no Back in Initials
    finishGame(560);
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Initials);
}

TEST_F(ScoresFlowTest, SR_UI_10_SavesOnceAndOpensHighScores) {
    finishGame(560);
    type("ABC");
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::HighScores);
    EXPECT_EQ(api.contexts.back(), InputContext::HighScores);
    EXPECT_EQ(saves, 1);
    app.update(press(Action::Select));  // Select does nothing here
    app.update(InputFrame{});
    EXPECT_EQ(saves, 1);
    EXPECT_EQ(scores.table(kGameIdSnake).count, 1U);
}

TEST_F(ScoresFlowTest, SR_UI_10_TwoSelectsInOneTickSaveOnce) {
    finishGame(560);
    type("ABC");
    InputFrame frame = press(Action::Select);
    frame.events[1] = frame.events[0];
    frame.count = 2;
    app.update(frame);
    EXPECT_EQ(saves, 1);
    EXPECT_EQ(scores.table(kGameIdSnake).count, 1U);
}

TEST_F(ScoresFlowTest, SR_UI_10_SecondRoundStartsWithEmptySlots) {
    finishGame(560);
    type("ABC");
    app.update(press(Action::Select));
    app.update(press(Action::Back));
    finishGame(700);
    ASSERT_EQ(app.state(), AppState::Initials);
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::Initials);
}

TEST_F(ScoresFlowTest, SR_UI_10_NewEntryIsHighlighted) {
    addSnake(900);
    addSnake(100);
    finishGame(560);
    type("NEW");
    app.update(press(Action::Select));
    app.render(api);
    EXPECT_TRUE(drew(api, "56,36:NEW"));
    EXPECT_EQ(api.cells[36][34], assets::kTileMenuGreen);  // rank 2 is on row 36
    EXPECT_EQ(api.cells[36][45], assets::kTileMenuBand);
    EXPECT_EQ(api.cells[34][45], 0);  // the other rows are not highlighted
    EXPECT_EQ(api.cells[38][45], 0);
}

TEST_F(ScoresFlowTest, SR_UI_10_HighlightStaysWithItsGameAndEndsOnReopen) {
    finishGame(560);
    type("NEW");
    app.update(press(Action::Select));
    app.update(press(Action::Right));  // to Breakout
    app.render(api);
    EXPECT_NE(api.cells[34][45], assets::kTileMenuBand);
    app.update(press(Action::Left));
    app.render(api);
    EXPECT_EQ(api.cells[34][45], assets::kTileMenuBand);
    app.update(press(Action::Back));
    app.update(press(Action::Up));  // wraps from Snake to SCORES
    app.update(press(Action::Select));
    ASSERT_EQ(app.state(), AppState::HighScores);
    app.render(api);
    EXPECT_NE(api.cells[34][45], assets::kTileMenuBand);
}

TEST_F(ScoresFlowTest, SR_UI_12_LeftAndRightWrapBetweenGames) {
    openFromMenu();
    app.render(api);
    EXPECT_TRUE(drew(api, "54,20:SNAKE"));
    app.update(press(Action::Right));
    app.render(api);
    EXPECT_TRUE(drew(api, "48,20:BREAKOUT"));
    app.update(press(Action::Right));  // wraps
    app.render(api);
    EXPECT_TRUE(drew(api, "54,20:SNAKE"));
    app.update(press(Action::Left));  // wraps
    app.render(api);
    EXPECT_TRUE(drew(api, "48,20:BREAKOUT"));
}

TEST_F(ScoresFlowTest, SR_UI_12_EachGameShowsItsOwnRows) {
    addSnake(900);
    ASSERT_TRUE(scores.insert(kGameIdBreakout, {'B', 'R', 'K'}, 40).has_value());
    openFromMenu();
    app.render(api);
    EXPECT_TRUE(drew(api, "56,34:OLD"));
    app.update(press(Action::Right));
    app.render(api);
    EXPECT_TRUE(drew(api, "56,34:BRK"));
    EXPECT_TRUE(drew(api, "80,34:000040"));
    EXPECT_FALSE(drew(api, "56,34:OLD"));
}

TEST_F(ScoresFlowTest, SR_UI_12_BackGoesToMenu) {
    openFromMenu();
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(api.contexts.back(), InputContext::Menu);
}

TEST_F(ScoresFlowTest, SR_FW_10_TieRanksBelowOlderEntry) {
    addSnake(560);
    finishGame(560);
    app.render(api);
    EXPECT_TRUE(drew(api, "49,28:SNAKE - SCORE 000560 - RANK 2"));
    type("NEW");
    app.update(press(Action::Select));
    app.render(api);
    EXPECT_TRUE(drew(api, "56,34:OLD"));
    EXPECT_TRUE(drew(api, "56,36:NEW"));
    EXPECT_EQ(api.cells[36][34], assets::kTileMenuGreen);
}

TEST_F(ScoresFlowTest, SR_FW_10_EleventhEntryDropsTheLowest) {
    for (std::uint32_t i = 0; i < HISCORE_SLOTS; ++i) {
        addSnake(1000 - i * 100);  // 1000 down to 100
    }
    finishGame(150);
    type("NEW");
    app.update(press(Action::Select));
    app.render(api);
    ASSERT_EQ(scores.table(kGameIdSnake).count, HISCORE_SLOTS);
    EXPECT_TRUE(drew(api, "56,52:NEW"));  // rank 10
    EXPECT_TRUE(drew(api, "37,52:10"));
    EXPECT_TRUE(drew(api, "80,52:000150"));
    EXPECT_FALSE(drew(api, "80,52:000100"));
}

TEST_F(ScoresFlowTest, Sec311_Screen7GoldenTilemap) {
    addSnake(900);
    finishGame(560);
    type("HU");
    app.render(api);

    FakeEngineApi golden;
    goldenBase(golden, "A-Z TYPE  BACKSPACE DELETE  ENTER CONFIRM");
    golden.drawText(34, 18, "NEW HIGH SCORE!", TextScale::X4);
    golden.drawText(49, 28, "SNAKE - SCORE 000560 - RANK 2", TextScale::X1);
    golden.fillRect(16, 32, 96, 1, assets::kTileDivider);
    golden.fillRect(49, 38, 6, 8, assets::kTileMenuBand);
    golden.fillRect(59, 38, 6, 8, assets::kTileMenuBand);
    golden.fillRect(69, 38, 6, 8, assets::kTileMenuBand);
    golden.fillRect(49, 46, 6, 1, assets::kTileDivider);
    golden.fillRect(59, 46, 6, 1, assets::kTileDivider);
    golden.fillRect(69, 46, 6, 1, assets::kTileMenuGreen);  // the third slot is current
    golden.drawText(50, 40, "H", TextScale::X4);
    golden.drawText(60, 40, "U", TextScale::X4);
    golden.drawText(54, 52, "ENTER YOUR INITIALS", TextScale::X1);
    EXPECT_EQ(api.cells, golden.cells);
}

TEST_F(ScoresFlowTest, Sec311_Screen8GoldenTilemap) {
    addSnake(900);
    addSnake(100);
    openFromMenu();
    app.render(api);

    FakeEngineApi golden;
    goldenBase(golden, "</> SWITCH GAME  ESC BACK  M SOUND: ON");
    golden.drawText(42, 6, "HIGH SCORES", TextScale::X4);
    golden.fillRect(16, 16, 96, 1, assets::kTileDivider);
    golden.drawText(40, 20, "<", TextScale::X4);
    golden.drawText(54, 20, "SNAKE", TextScale::X4);
    golden.drawText(84, 20, ">", TextScale::X4);
    golden.drawText(36, 30, "RANK", TextScale::X1);
    golden.drawText(56, 30, "NAME", TextScale::X1);
    golden.drawText(80, 30, "SCORE", TextScale::X1);
    golden.fillRect(34, 32, 60, 1, assets::kTileDivider);
    golden.drawText(38, 34, "1", TextScale::X1);
    golden.drawText(56, 34, "OLD", TextScale::X1);
    golden.drawText(80, 34, "000900", TextScale::X1);
    golden.drawText(38, 36, "2", TextScale::X1);
    golden.drawText(56, 36, "OLD", TextScale::X1);
    golden.drawText(80, 36, "000100", TextScale::X1);
    EXPECT_EQ(api.cells, golden.cells);
}

}  // namespace
}  // namespace arcade
