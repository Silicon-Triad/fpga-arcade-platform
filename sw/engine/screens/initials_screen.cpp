#include "engine/screens/initials_screen.hpp"

#include <algorithm>
#include <cstdio>

#include "assets/game_tiles.hpp"
#include "assets/tile_ids.hpp"
#include "engine/ui_layout.hpp"

namespace arcade {
namespace {

// Section 3.1.1 screen 7; the divider and slot widths are measured from Figure 3.7.
constexpr std::string_view kTitle = "NEW HIGH SCORE!";
constexpr std::string_view kPrompt = "ENTER YOUR INITIALS";
constexpr int kTitleRow = 18;
constexpr int kInfoRow = 28;
constexpr int kDividerRow = 32;
constexpr int kDividerCol = 16;
constexpr int kDividerWidth = 96;
constexpr int kFirstLetterCol = 50;
constexpr int kSlotSpacing = 10;
constexpr int kLetterRow = 40;
constexpr int kSlotInset = 1;  // the background starts one column left of the letter
constexpr int kSlotWidth = 6;
constexpr int kSlotRow = 38;
constexpr int kSlotHeight = 8;  // rows 38-45
constexpr int kUnderlineRow = 46;
constexpr int kPromptRow = 52;
constexpr std::size_t kLineMax = 48;  // DESIGN CHOICE: longest composed line, no heap (R-4.1.6)

}  // namespace

void InitialsScreen::begin(std::string_view name, std::uint32_t score, std::uint8_t rank) noexcept {
    name_ = name;
    score_ = score;
    rank_ = rank;
    letters_.fill(0);
    count_ = 0;
    confirmed_ = false;
}

void InitialsScreen::update(const InputFrame& input) noexcept {
    for (std::size_t i = 0; i < input.count && !confirmed_; ++i) {
        const InputEvent& event = input.events[i];
        if (event.type != InputEventType::KeyDown) {
            continue;
        }
        if (event.action == Action::Char) {  // SR-UI-10, SR-INP-11
            if (count_ < INITIALS_LEN && event.ch >= 'A' && event.ch <= 'Z') {
                letters_[count_] = event.ch;
                ++count_;
            }
        } else if (event.action == Action::Delete) {  // SR-UI-10
            if (count_ > 0) {
                --count_;
                letters_[count_] = 0;
            }
        } else if (event.action == Action::Select) {  // SR-UI-10: exactly three letters
            confirmed_ = count_ == INITIALS_LEN;
        }
    }
}

bool InitialsScreen::takeConfirmed() noexcept {
    const bool confirmed = confirmed_;
    confirmed_ = false;
    return confirmed;
}

void InitialsScreen::render(EngineApi& api) noexcept {
    api.clear(assets::kFontFirstTile);
    drawFrame(api);
    drawCentred(api, kTitleRow, kTitle, TextScale::X4);
    std::array<char, kLineMax> line{};
    const int length = std::snprintf(line.data(), line.size(), "%.*s - SCORE %06u - RANK %u",
                                     static_cast<int>(name_.size()), name_.data(),
                                     static_cast<unsigned>(score_), static_cast<unsigned>(rank_));
    drawCentred(
        api, kInfoRow,
        {line.data(), std::min(static_cast<std::size_t>(std::max(length, 0)), line.size() - 1)},
        TextScale::X1);
    drawDivider(api, kDividerCol, kDividerRow, kDividerWidth);

    // With every slot full the last stays current: Delete acts there.
    const std::size_t current = std::min(count_, INITIALS_LEN - 1);
    for (std::size_t i = 0; i < INITIALS_LEN; ++i) {
        const int letterCol = kFirstLetterCol + static_cast<int>(i) * kSlotSpacing;
        const int slotCol = letterCol - kSlotInset;
        api.fillRect(slotCol, kSlotRow, kSlotWidth, kSlotHeight, assets::kTileMenuBand);
        api.fillRect(slotCol, kUnderlineRow, kSlotWidth, 1,
                     i == current ? assets::kTileMenuGreen : assets::kTileDivider);
        if (i < count_) {
            api.drawText(letterCol, kLetterRow, {&letters_[i], 1}, TextScale::X4);
        }
    }
    drawCentred(api, kPromptRow, kPrompt, TextScale::X1);
    drawHintBar(api, HintScreen::Initials, false);  // SR-UI-13: this hint has no sound state
}

}  // namespace arcade
