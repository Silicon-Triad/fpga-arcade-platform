// Golden-tilemap tests for the common layout and hint bar (issue #9, SR-UI-13, section 3.1.1).
#include "engine/ui_layout.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <string>

#include "assets/text_tiles.hpp"
#include "assets/tile_ids.hpp"

namespace arcade {
namespace {

using assets::kTileAccent;
using assets::kTileBoxBlue;
using assets::kTileBoxRed;
using assets::kTileDivider;
using assets::kTileFrame;

constexpr TileIndex kUntouched = 255;

/// ArcadeApi that records drawing into a tilemap.
class FakeApi : public ArcadeApi {
  public:
    FakeApi() { clear(kUntouched); }

    TileIndex& at(int col, int row) {
        return cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)];
    }

    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        return fillRect(col, row, 1, 1, tile);
    }
    ApiStatus fillRect(int col, int row, int width, int height, TileIndex tile) noexcept override {
        if (col < 0 || row < 0 || col + width > kGridCols || row + height > kGridRows) {
            return ApiStatus::OutOfRange;
        }
        for (int y = row; y < row + height; ++y) {
            for (int x = col; x < col + width; ++x) {
                at(x, y) = tile;
            }
        }
        return ApiStatus::Ok;
    }
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override {
        return assets::textToTiles(
            col, row, text, scale,
            [](void* self, int x, int y, TileIndex tile) noexcept {
                static_cast<FakeApi*>(self)->at(x, y) = tile;
            },
            this);
    }
    void clear(TileIndex tile) noexcept override {
        for (auto& row : cells) {
            row.fill(tile);
        }
    }
    ApiStatus present() noexcept override { return ApiStatus::Ok; }
    InputFrame pollInput() noexcept override { return {}; }
    void playSound(EffectId) noexcept override {}
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return 0; }

    std::array<std::array<TileIndex, kGridCols>, kGridRows> cells{};
};

/// Reads scale-1 text from `length` cells back as a string.
std::string textAt(FakeApi& api, int col, int row, int length) {
    std::string text;
    for (int i = 0; i < length; ++i) {
        text += static_cast<char>(api.at(col + i, row) + 0x20);
    }
    return text;
}

TEST(UiLayout, Sec311_FrameOnEdgesWithAccentEvery8Columns) {
    FakeApi api;
    EXPECT_EQ(drawFrame(api), ApiStatus::Ok);
    for (int col = 0; col < kGridCols; ++col) {
        const TileIndex expected = col % 8 == 0 ? kTileAccent : kTileFrame;
        EXPECT_EQ(api.at(col, 0), expected) << col;
        EXPECT_EQ(api.at(col, 95), expected) << col;
    }
    for (int row = 1; row < 95; ++row) {
        EXPECT_EQ(api.at(0, row), kTileFrame) << row;
        EXPECT_EQ(api.at(127, row), kTileFrame) << row;
        EXPECT_EQ(api.at(1, row), kUntouched) << row;
        EXPECT_EQ(api.at(126, row), kUntouched) << row;
    }
}

struct HintCase {
    HintScreen screen;
    int row;
    std::string on;
    std::string off;
};

TEST(UiLayout, SR_UI_13_EveryScreenHintAtItsRowWithSoundState) {
    const std::array<HintCase, 7> withSound{{
        {HintScreen::Menu, 93,
         "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: ON  HOLD ESC: SHUT DOWN",
         "^/v CHOOSE  ENTER PLAY  H HIGH SCORES  M SOUND: OFF  HOLD ESC: SHUT DOWN"},
        {HintScreen::Snake, 94, "ARROWS TURN  P PAUSE  ESC MENU  M SOUND: ON",
         "ARROWS TURN  P PAUSE  ESC MENU  M SOUND: OFF"},
        {HintScreen::Breakout, 94, "</> MOVE  SPACE LAUNCH  P PAUSE  ESC MENU  M SOUND: ON",
         "</> MOVE  SPACE LAUNCH  P PAUSE  ESC MENU  M SOUND: OFF"},
        {HintScreen::Paused, 94, "P RESUME  ESC MENU  M SOUND: ON",
         "P RESUME  ESC MENU  M SOUND: OFF"},
        {HintScreen::GameOver, 94, "ENTER PLAY AGAIN  ESC MENU  M SOUND: ON",
         "ENTER PLAY AGAIN  ESC MENU  M SOUND: OFF"},
        {HintScreen::HighScores, 93, "</> SWITCH GAME  ESC BACK  M SOUND: ON",
         "</> SWITCH GAME  ESC BACK  M SOUND: OFF"},
        {HintScreen::Initials, 93, "A-Z TYPE  BACKSPACE DELETE  ENTER CONFIRM",
         "A-Z TYPE  BACKSPACE DELETE  ENTER CONFIRM"},
    }};
    for (const HintCase& c : withSound) {
        for (const bool sound : {true, false}) {
            const std::string& expected = sound ? c.on : c.off;
            FakeApi api;
            EXPECT_EQ(drawHintBar(api, c.screen, sound), ApiStatus::Ok);
            EXPECT_EQ(textAt(api, 2, c.row, static_cast<int>(expected.size())), expected);
            EXPECT_EQ(api.at(1, c.row), 0);  // band covers columns 1-126
            EXPECT_EQ(api.at(126, c.row), 0);
            EXPECT_EQ(api.at(0, c.row), kUntouched);
            EXPECT_EQ(api.at(127, c.row), kUntouched);
        }
    }
}

TEST(UiLayout, SR_UI_13_ShutdownConfirmHintOnRow93) {
    FakeApi api;
    drawHintBar(api, HintScreen::ShutdownConfirm, true);
    EXPECT_EQ(textAt(api, 2, 93, 25), "ENTER CONFIRM  ESC CANCEL");
}

TEST(UiLayout, Sec311_NoKeyboardHintRowIsEmpty) {
    FakeApi api;
    EXPECT_EQ(drawHintBar(api, HintScreen::NoKeyboard, true), ApiStatus::Ok);
    for (int col = 1; col <= 126; ++col) {
        EXPECT_EQ(api.at(col, 94), 0);
    }
}

TEST(UiLayout, Sec311_BootAndSafeOffHaveNoHintBar) {
    for (const HintScreen screen : {HintScreen::Boot, HintScreen::SafeOff}) {
        FakeApi api;
        EXPECT_EQ(drawHintBar(api, screen, true), ApiStatus::Ok);
        EXPECT_EQ(api.at(2, 93), kUntouched);
        EXPECT_EQ(api.at(2, 94), kUntouched);
    }
}

struct BoxCase {
    int col, row, width, height;
    TileIndex tile;
};

TEST(UiLayout, Sec311_ScreenBoxesMatchGoldenPositions) {
    const std::array<BoxCase, 3> boxes{{
        {40, 38, 48, 20, kTileBoxBlue},  // screens 5 and 10a: columns 40-87, rows 38-57
        {36, 34, 56, 28, kTileBoxRed},   // screen 6: columns 36-91, rows 34-61
        {30, 38, 68, 20, kTileBoxRed},   // screen 9: columns 30-97, rows 38-57
    }};
    for (const BoxCase& b : boxes) {
        FakeApi api;
        EXPECT_EQ(drawBox(api, b.col, b.row, b.width, b.height, b.tile), ApiStatus::Ok);
        for (int y = 0; y < kGridRows; ++y) {
            for (int x = 0; x < kGridCols; ++x) {
                const bool inside =
                    x >= b.col && x < b.col + b.width && y >= b.row && y < b.row + b.height;
                const bool edge = x == b.col || x == b.col + b.width - 1 || y == b.row ||
                                  y == b.row + b.height - 1;
                const TileIndex expected = !inside ? kUntouched : edge ? b.tile : TileIndex{0};
                ASSERT_EQ(api.at(x, y), expected) << x << "," << y;
            }
        }
    }
}

TEST(UiLayout, Sec311_BadBoxDrawsNothing) {
    FakeApi api;
    EXPECT_EQ(drawBox(api, 120, 0, 20, 20, kTileBoxBlue), ApiStatus::OutOfRange);
    EXPECT_EQ(api.at(120, 0), kUntouched);
}

TEST(UiLayout, Sec311_MenuDividerRow19Columns16To111) {
    FakeApi api;
    EXPECT_EQ(drawDivider(api, 16, 19, 96), ApiStatus::Ok);
    EXPECT_EQ(api.at(15, 19), kUntouched);
    EXPECT_EQ(api.at(16, 19), kTileDivider);
    EXPECT_EQ(api.at(111, 19), kTileDivider);
    EXPECT_EQ(api.at(112, 19), kUntouched);
}

TEST(UiLayout, Sec311_CentredTextMatchesSrsColumns) {
    EXPECT_EQ(centredCol(11, TextScale::X4), 42);  // "FPGA ARCADE": columns 42-85
    EXPECT_EQ(centredCol(18, TextScale::X4), 28);  // "SAFE TO SWITCH OFF": columns 28-99
    EXPECT_EQ(rightAlignedCol(9, 125), 117);       // "HI 000560" ends at column 125
}

TEST(UiLayout, SR_FW_12_ScaleFourTitleOccupiesSrsRows) {
    FakeApi api;
    EXPECT_EQ(api.drawText(centredCol(11, TextScale::X4), 34, "FPGA ARCADE", TextScale::X4),
              ApiStatus::Ok);
    EXPECT_EQ(api.at(41, 34), kUntouched);
    EXPECT_NE(api.at(42, 34), kUntouched);
    EXPECT_NE(api.at(85, 37), kUntouched);
    EXPECT_EQ(api.at(86, 37), kUntouched);
    EXPECT_EQ(api.at(42, 38), kUntouched);
}

}  // namespace
}  // namespace arcade
