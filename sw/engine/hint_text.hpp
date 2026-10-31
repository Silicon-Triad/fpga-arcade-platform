/// @file
/// Hint-bar text of each screen (SR-UI-13, srs-huzaifa-rev3.md section 3.1.1).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arcade {

/// Screens that have a hint bar, or none (section 3.1.1 screen list).
enum class HintScreen : std::uint8_t {
    Boot,
    Menu,
    Snake,
    Breakout,
    Paused,
    GameOver,
    Initials,
    HighScores,
    NoKeyboard,
    ShutdownConfirm,
    SafeOff,
};

/// Hint bar row during play, screens 3-6 and 9 (section 3.1.1 common layout).
constexpr int kHintRowPlay = 94;
/// Hint bar row on other screens (section 3.1.1 common layout).
constexpr int kHintRowOther = 93;

/// Longest hint: the band over columns 1-126 less the leading blank (section 3.1.1).
constexpr std::size_t kHintMax = 125;

/// One screen's hint text and row, in a fixed buffer: no heap in a tick (R-4.1.6).
struct HintBar {
    int row = 0;
    std::array<char, kHintMax> chars{};
    std::size_t length = 0;

    /// The text as a view into this object.
    std::string_view text() const noexcept { return {chars.data(), length}; }
};

/// Returns the hint bar of `screen` with the current sound state (SR-UI-13, SR-FW-11), or nullopt
/// for screens 1 and 10b, which have none (section 3.1.1).
std::optional<HintBar> hintBarFor(HintScreen screen, bool soundOn) noexcept;

}  // namespace arcade
