// Unit tests for the hold, the confirmation and the high-score flush (issue #15, SR-UI-14,
// SR-FW-13, section 3.1.1 screen 10a).
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <string>

#include "assets/tile_ids.hpp"
#include "engine/app.hpp"
#include "engine/high_score_store.hpp"
#include "fake_engine_api.hpp"

namespace arcade {
namespace {

constexpr int kHoldTicks = 180;  // App. A SHUTDOWN_HOLD

class StillGame final : public Game {
  public:
    void init() noexcept override {}
    void update(const InputFrame&) noexcept override {}
    void render(ArcadeApi&) noexcept override {}
    bool isOver() const noexcept override { return false; }
    std::uint32_t score() const noexcept override { return 0; }
};

std::unique_ptr<Game> makeGame() { return std::make_unique<StillGame>(); }

constexpr std::array<GameRegistryRow, 1> kRows{{{kGameIdSnake, "SNAKE", 0, &makeGame}}};

std::uint32_t noScore(std::uint8_t) { return 0; }

InputFrame heldBack() {
    InputFrame frame{};
    frame.held[static_cast<std::size_t>(Action::Back)] = true;
    return frame;
}

InputFrame press(Action action, bool held = false) {
    InputFrame frame{};
    frame.events[0] = InputEvent{InputEventType::KeyDown, action, 0};
    frame.count = 1;
    frame.held[static_cast<std::size_t>(action)] = held;
    return frame;
}

class ShutdownHoldTest : public ::testing::Test {
  protected:
    ShutdownHoldTest() : app(api, kRows.data(), kRows.size(), &noScore) {
        app.setState(AppState::Menu);
    }

    // KeyDown, then `ticks` more ticks with the key held (SR-UI-14: timed from KeyDown).
    void hold(int ticks) {
        app.update(press(Action::Back, true));
        for (int i = 0; i < ticks; ++i) {
            app.update(heldBack());
        }
    }

    bool hasText(const std::string& text) const {
        return std::find(api.texts.begin(), api.texts.end(), text) != api.texts.end();
    }

    FakeEngineApi api;
    App app;
};

TEST_F(ShutdownHoldTest, SR_UI_14_Hold179TicksShowsNoBox) {
    hold(kHoldTicks - 1);
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_Hold180TicksShowsBox) {
    hold(kHoldTicks);
    EXPECT_EQ(app.state(), AppState::ShutdownConfirm);
    EXPECT_EQ(api.contexts.back(), InputContext::ShutdownConfirm);
}

TEST_F(ShutdownHoldTest, SR_UI_14_ReleaseBefore180DoesNothingAndRestartsTheCount) {
    hold(kHoldTicks - 1);
    app.update(InputFrame{});
    hold(kHoldTicks - 1);
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_HeldWithoutKeyDownNeverOpens) {
    for (int i = 0; i < 2 * kHoldTicks; ++i) {
        app.update(heldBack());
    }
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_DoesNotRepeatTriggerAfterCancel) {
    hold(kHoldTicks);
    app.update(press(Action::Back, true));  // cancels; the key stays down
    ASSERT_EQ(app.state(), AppState::Menu);
    for (int i = 0; i < 2 * kHoldTicks; ++i) {
        app.update(heldBack());
    }
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_HoldDoesNotContinueAfterLeavingTheMenu) {
    hold(kHoldTicks - 1);
    app.update(press(Action::Select));
    ASSERT_EQ(app.state(), AppState::Playing);
    app.update(press(Action::Back, true));  // back on the menu with the key down
    ASSERT_EQ(app.state(), AppState::Menu);
    app.update(heldBack());
    EXPECT_EQ(app.state(), AppState::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_BackCancels) {
    hold(kHoldTicks);
    app.update(press(Action::Back));
    EXPECT_EQ(app.state(), AppState::Menu);
    EXPECT_EQ(api.contexts.back(), InputContext::Menu);
}

TEST_F(ShutdownHoldTest, SR_UI_14_SelectConfirms) {
    hold(kHoldTicks);
    app.update(press(Action::Select));
    EXPECT_EQ(app.state(), AppState::ShuttingDown);
}

TEST_F(ShutdownHoldTest, SR_UI_14_BoxMatchesScreen10aOverTheMenu) {
    hold(kHoldTicks);
    app.render(api);
    const auto cell = [this](int col, int row) {
        return api.cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
    };
    EXPECT_EQ(cell(40, 38), assets::kTileBoxBlue);
    EXPECT_EQ(cell(87, 57), assets::kTileBoxBlue);
    EXPECT_NE(cell(39, 38), assets::kTileBoxBlue);
    EXPECT_NE(cell(88, 57), assets::kTileBoxBlue);
    EXPECT_EQ(cell(0, 1), assets::kTileFrame);  // the menu stays in place
    EXPECT_TRUE(hasText("41:SHUT DOWN?"));
    EXPECT_TRUE(hasText("48:ENTER YES"));
    EXPECT_TRUE(hasText("50:ESC NO"));
    EXPECT_TRUE(hasText("93:ENTER CONFIRM  ESC CANCEL"));
}

class CountingFileSystem final : public FileSystem {
  public:
    std::optional<std::string> readAll(const std::string&) noexcept override {
        return std::nullopt;
    }
    int openForWrite(const std::string&) noexcept override { return ok ? 1 : -1; }
    bool writeAll(int, std::string_view) noexcept override { return true; }
    bool sync(int) noexcept override { return true; }
    bool close(int) noexcept override { return true; }
    bool rename(const std::string&, const std::string&) noexcept override {
        ++renames;
        return true;
    }
    bool ok = true;
    int renames = 0;
};

TEST(HighScoreStore, SR_FW_13_SavePendingWritesOnlyAnUnsavedInsert) {
    HighScoreStore store;
    CountingFileSystem fs;
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(fs.renames, 0);
    ASSERT_TRUE(store.insert(kGameIdSnake, {'A', 'B', 'C'}, 10).has_value());
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(fs.renames, 1);
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(fs.renames, 1);
}

TEST(HighScoreStore, SR_FW_13_SaveClearsThePendingSave) {
    HighScoreStore store;
    CountingFileSystem fs;
    ASSERT_TRUE(store.insert(kGameIdSnake, {'A', 'B', 'C'}, 10).has_value());
    EXPECT_EQ(store.save(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(fs.renames, 1);
}

TEST(HighScoreStore, SR_FW_13_FailedSaveStaysPending) {
    HighScoreStore store;
    CountingFileSystem fs;
    ASSERT_TRUE(store.insert(kGameIdSnake, {'A', 'B', 'C'}, 10).has_value());
    fs.ok = false;
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::OpenFailed);
    fs.ok = true;
    EXPECT_EQ(store.savePending(fs, "f"), SaveStatus::Ok);
    EXPECT_EQ(fs.renames, 1);
}

}  // namespace
}  // namespace arcade
