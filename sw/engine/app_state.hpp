/// @file
/// The application states (SRS Chapter 5 application state machine, App. A appState).
#pragma once

#include <cstddef>
#include <cstdint>

namespace arcade {

/// The Game Engine is always in exactly one of these states (App. A appState).
enum class AppState : std::uint8_t {
    Boot,
    HardwareNotReady,
    Menu,
    HighScores,
    ShutdownConfirm,
    ShuttingDown,
    Playing,
    Paused,
    KeyboardLost,
    GameOver,
    Initials,
};

/// Number of AppState values.
constexpr std::size_t kAppStateCount = 11;
static_assert(static_cast<std::size_t>(AppState::Initials) + 1 == kAppStateCount, "App. A");

}  // namespace arcade
