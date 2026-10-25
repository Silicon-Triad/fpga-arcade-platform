#pragma once

#include <linux/input.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace arcade::uinput {

/// PR-06: number of press/release pairs injected (30 keys/s for 60 s).
constexpr std::size_t kPr06Pairs = 1800;
/// PR-06: key presses per second.
constexpr unsigned kPr06KeysPerSecond = 30;

/// Keys pressed in turn, one per pair: the letters A to Z. Letters only, so `uinput_inject`
/// cannot run commands if its keys reach a terminal (no Enter). Not an SRS value.
constexpr std::array<std::uint16_t, 26> kInjectKeys = {
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
    KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z};

/// What to inject: `pairs` press/release pairs at `keysPerSecond` presses per second (PR-06).
struct InjectPlan {
    std::size_t pairs = kPr06Pairs;
    unsigned keysPerSecond = kPr06KeysPerSecond;
};

/// Key code of pair `i`: kInjectKeys in turn.
std::uint16_t keyForPair(std::size_t i);

/// Time of the press of pair `i`, from the start: i / keysPerSecond (PR-06). `keysPerSecond`
/// must not be 0.
std::chrono::nanoseconds pressAt(const InjectPlan& plan, std::size_t i);

/// Time of the release of pair `i`: halfway between its press and the next press, so a key is
/// always released before the next one is pressed. Not an SRS value.
std::chrono::nanoseconds releaseAt(const InjectPlan& plan, std::size_t i);

/// Time of the last release, or 0 for an empty plan.
std::chrono::nanoseconds planDuration(const InjectPlan& plan);

/// Runs `plan` (PR-06): for each pair, `waitUntil(pressAt(i))` then `send(code, 1)`, then
/// `waitUntil(releaseAt(i))` then `send(code, 0)`. Stops and returns false at the first `send`
/// that returns false.
template <typename WaitUntil, typename Send>
bool runPlan(const InjectPlan& plan, WaitUntil&& waitUntil, Send&& send) {
    for (std::size_t i = 0; i < plan.pairs; ++i) {
        const std::uint16_t code = keyForPair(i);
        waitUntil(pressAt(plan, i));
        if (!send(code, 1)) {
            return false;
        }
        waitUntil(releaseAt(plan, i));
        if (!send(code, 0)) {
            return false;
        }
    }
    return true;
}

}  // namespace arcade::uinput
