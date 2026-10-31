#include "engine/game_menu.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "assets/game_tiles.hpp"
#include "engine/preview_api.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 2.
constexpr std::string_view kScoresName = "SCORES";
constexpr std::string_view kLogo = "FPGA ARCADE";
constexpr std::string_view kSubtitle = "SILICON TRIAD - DE1-SOC - 1024X768";
constexpr std::string_view kPrompt = "PRESS ENTER TO PLAY";
constexpr int kLogoRow = 6;
constexpr int kSubtitleRow = 16;
constexpr int kDividerRow = 19;
constexpr int kDividerCol = 16;
constexpr int kDividerWidth = 96;
constexpr int kItemCol = 13;
constexpr int kFirstItemRow = 28;
constexpr int kItemSpacing = 10;
constexpr int kBandCol = 6;
constexpr int kBandWidth = 50;
constexpr int kBandAbove = 2;
constexpr int kBandHeight = 8;
constexpr int kCursorCol = 8;
constexpr int kPromptRow = 62;
constexpr int kPanelCol = 60;
constexpr int kPanelRow = 24;
constexpr int kPanelWidth = 62;
constexpr int kPanelHeight = 54;
constexpr int kCaptionRow = 74;
constexpr int kStatusRow = 86;
constexpr std::size_t kLineMax = 48;  // DESIGN CHOICE: longest composed line, no heap (R-4.1.6)

// DESIGN CHOICE: how often the preview presses keys, so that games waiting for a key move.
constexpr std::uint32_t kPreviewKeyTicks = 60;

}  // namespace

MenuScreen::MenuScreen(std::vector<MenuGame> games, BestScoreFn bestScore)
    : games_(std::move(games)), bestScore_(bestScore) {
    // SR-FW-04: the menu order comes only from the registry.
    std::sort(games_.begin(), games_.end(), [](const MenuGame& a, const MenuGame& b) {
        return a.row->menu_order < b.row->menu_order;
    });
}

void MenuScreen::update(const InputFrame& input) noexcept {
    const std::size_t count = entryCount();
    for (std::size_t i = 0; i < input.count; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type != InputEventType::KeyDown) {
            continue;
        }
        if (event.action == Action::Up) {  // SR-UI-01: wraps at both ends
            selection_ = (selection_ + count - 1) % count;
            previewTicks_ = 0;
        } else if (event.action == Action::Down) {
            selection_ = (selection_ + 1) % count;
            previewTicks_ = 0;
        } else if (event.action == Action::Select) {  // SR-UI-03
            request_ = selection_ < games_.size() ? Request::StartGame : Request::Scores;
        }
    }
    movePreview();
}

// SR-UI-02: the preview runs the real game through the frozen interface, so a new game needs no
// engine change (QA-02). A stopped game is restarted and nudged with keys.
void MenuScreen::movePreview() noexcept {
    const MenuGame* selected = selectedGame();
    if (selected == nullptr) {
        return;
    }
    if (selected->game->isOver()) {
        selected->game->init();
        previewTicks_ = 0;
    }
    InputFrame demo{};
    if (previewTicks_ % kPreviewKeyTicks == 0) {
        demo.events[0] = InputEvent{InputEventType::KeyDown, Action::Right, 0};
        demo.events[1] = InputEvent{InputEventType::KeyDown, Action::Launch, 0};
        demo.count = 2;
    }
    selected->game->update(demo);
    ++previewTicks_;
}

void MenuScreen::render(EngineApi& api) noexcept {
    api.clear(assets::kFontFirstTile);
    drawFrame(api);
    api.drawText(centredCol(static_cast<int>(kLogo.size()), TextScale::X4), kLogoRow, kLogo,
                 TextScale::X4);
    api.drawText(centredCol(static_cast<int>(kSubtitle.size()), TextScale::X1), kSubtitleRow,
                 kSubtitle, TextScale::X1);
    drawDivider(api, kDividerCol, kDividerRow, kDividerWidth);

    const int selectedRow = kFirstItemRow + static_cast<int>(selection_) * kItemSpacing;
    api.fillRect(kBandCol, selectedRow - kBandAbove, kBandWidth, kBandHeight,
                 assets::kTileMenuBand);
    api.fillRect(kBandCol, selectedRow - kBandAbove, 1, kBandHeight, assets::kTileMenuGreen);
    api.drawText(kCursorCol, selectedRow, ">", TextScale::X4);
    for (std::size_t i = 0; i < entryCount(); ++i) {
        api.drawText(kItemCol, kFirstItemRow + static_cast<int>(i) * kItemSpacing, entryName(i),
                     TextScale::X4);
    }
    api.drawText(kItemCol, kPromptRow, kPrompt, TextScale::X1);

    drawBox(api, kPanelCol, kPanelRow, kPanelWidth, kPanelHeight, assets::kTileMenuGreen);
    std::array<char, kLineMax> line{};
    if (const MenuGame* selected = selectedGame()) {
        PreviewApi preview(api);
        selected->game->render(preview);
        const int length =
            std::snprintf(line.data(), line.size(), "BEST %06u - %.*s",
                          static_cast<unsigned>(bestScore_(selected->row->game_id)),
                          static_cast<int>(selected->row->name.size()), selected->row->name.data());
        api.drawText(kPanelCol + 2, kCaptionRow,
                     {line.data(), static_cast<std::size_t>(std::max(length, 0))}, TextScale::X1);
    }
    const int status = std::snprintf(line.data(), line.size(),
                                     "SOUND ON - %zu GAMES - TOP 10 SAVED", games_.size());
    api.drawText(kItemCol, kStatusRow, {line.data(), static_cast<std::size_t>(std::max(status, 0))},
                 TextScale::X1);
    drawHintBar(api, HintScreen::Menu, true);  // sound state arrives with #14
}

MenuScreen::Request MenuScreen::takeRequest() noexcept {
    const Request request = request_;
    request_ = Request::None;
    return request;
}

std::string_view MenuScreen::entryName(std::size_t index) const noexcept {
    return index < games_.size() ? games_[index].row->name : kScoresName;
}

const MenuGame* MenuScreen::selectedGame() const noexcept {
    return selection_ < games_.size() ? &games_[selection_] : nullptr;
}

}  // namespace arcade
