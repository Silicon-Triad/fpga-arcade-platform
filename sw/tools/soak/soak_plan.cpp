// Soak driver key plan (QA-05, issue #51).
#include "tools/soak/soak_plan.hpp"

#include <algorithm>
#include <cstdlib>
#include <random>

namespace arcade::soak {
namespace {

// Test choices, not SRS values: a human-like rate of about 2 keys per second.
constexpr std::uint64_t kTapMs = 80;       // a short key press
constexpr std::uint64_t kMenuGapMs = 400;  // between menu moves
constexpr std::uint64_t kSettleMs = 1500;  // after a screen change
constexpr std::uint64_t kPauseEveryMs = 60000;
constexpr std::uint64_t kPausedMs = 2000;
constexpr std::uint64_t kServeEveryMs = 3000;  // Breakout end phase: Launch only
constexpr std::uint64_t kEscHoldMs = 100;      // far below SHUTDOWN_HOLD (3 s, SR-UI-14)
constexpr std::uint64_t kMsPerSecond = 1000;
constexpr std::uint64_t kMsPerMinute = 60000;
// Letters only, no M (Global Mute) and no P (Pause), so a key that lands on another screen
// does nothing harmful (README).
constexpr std::array<std::uint16_t, 3> kInitials = {KEY_S, KEY_K, KEY_A};

class Builder {
  public:
    explicit Builder(std::uint32_t seed) : rng_(seed) {}

    void tap(std::uint16_t key, std::uint64_t holdMs, std::uint64_t gapMs, const char* note) {
        steps.push_back({t, key, 1, note});
        steps.push_back({t + holdMs, key, 0, note});
        t += holdMs + gapMs;
    }
    void wait(std::uint64_t ms) { t += ms; }
    std::uint64_t between(std::uint64_t lo, std::uint64_t hi) {
        return std::uniform_int_distribution<std::uint64_t>(lo, hi)(rng_);
    }

    std::vector<Step> steps;
    std::uint64_t t = 0;

  private:
    std::mt19937 rng_;
};

void playSnake(Builder& b, std::uint64_t untilMs) {
    constexpr std::array<std::uint16_t, 4> arrows = {KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT};
    std::uint64_t nextPause = b.t + kPauseEveryMs;
    while (b.t < untilMs) {
        if (b.t >= nextPause) {  // SR-UI-05, SR-UI-06: pause and resume
            b.tap(KEY_P, kTapMs, kPausedMs, "snake pause");
            b.tap(KEY_P, kTapMs, kSettleMs, "snake resume");
            nextPause = b.t + kPauseEveryMs;
            continue;
        }
        b.tap(arrows[static_cast<std::size_t>(b.between(0, 3))], kTapMs, b.between(250, 750),
              "snake play");
    }
}

void playBreakout(Builder& b, std::uint64_t untilMs) {
    b.tap(KEY_SPACE, kTapMs, 500, "breakout launch");
    std::uint64_t nextPause = b.t + kPauseEveryMs;
    while (b.t < untilMs) {
        if (b.t >= nextPause) {
            b.tap(KEY_P, kTapMs, kPausedMs, "breakout pause");
            b.tap(KEY_P, kTapMs, kSettleMs, "breakout resume");
            nextPause = b.t + kPauseEveryMs;
            continue;
        }
        if (b.between(0, 7) == 0) {  // relaunch after a lost ball
            b.tap(KEY_SPACE, kTapMs, 300, "breakout launch");
            continue;
        }
        const std::uint16_t key = b.between(0, 1) == 0 ? KEY_LEFT : KEY_RIGHT;
        b.tap(key, b.between(100, 500), b.between(100, 400), "breakout play");
    }
}

void buildRound(Builder& b, std::uint32_t round, std::uint32_t& selection, const PlanOptions& opt) {
    const std::uint32_t game = gameOfRound(round);
    // SR-UI-01: Down moves the selection one entry, wrapping; Enter starts it (SR-UI-03).
    const std::uint32_t moves = (game + kMenuEntries - selection) % kMenuEntries;
    for (std::uint32_t i = 0; i < moves; ++i) {
        b.tap(KEY_DOWN, kTapMs, kMenuGapMs, "menu down");
    }
    selection = game;
    b.tap(KEY_ENTER, kTapMs, kSettleMs, game == 0 ? "menu start snake" : "menu start breakout");

    const std::uint64_t playEnd = b.t + std::uint64_t{opt.playSeconds} * kMsPerSecond;
    if (game == 0) {
        playSnake(b, playEnd);
        b.wait(std::uint64_t{opt.endSeconds} * kMsPerSecond);  // no steering: hits a wall
    } else {
        playBreakout(b, playEnd);
        const std::uint64_t endAt = b.t + std::uint64_t{opt.endSeconds} * kMsPerSecond;
        while (b.t < endAt) {  // launch every ball, never move: lives run out (SR-GAM-13)
            b.tap(KEY_SPACE, kTapMs, kServeEveryMs, "breakout end");
        }
    }
    if (initialsInRound(round)) {
        // SR-UI-08, SR-UI-10: on Initials these are saved and HighScores opens.
        for (const std::uint16_t key : kInitials) {
            b.tap(key, kTapMs, 500, "initials");
        }
        b.tap(KEY_ENTER, kTapMs, 3000, "initials enter");
    }
    // Back: to the menu from Playing, Paused, GameOver, Initials or HighScores (SR-UI-07,
    // SR-UI-09, SR-UI-12).
    b.tap(KEY_ESC, kEscHoldMs, 2000, "back to menu");
}

std::vector<Step> build(const PlanOptions& opt, std::uint32_t* rounds) {
    Builder b(opt.seed);
    const std::uint64_t totalMs = std::uint64_t{opt.minutes} * kMsPerMinute;
    std::uint32_t selection = 0;  // SNAKE is selected when the menu first opens
    std::uint32_t r = 0;
    for (;; ++r) {
        Builder trial = b;
        std::uint32_t sel = selection;
        buildRound(trial, r, sel, opt);
        if (trial.t > totalMs) {
            break;
        }
        b = trial;
        selection = sel;
    }
    if (rounds != nullptr) {
        *rounds = r;
    }
    return b.steps;
}

bool parseUint(const std::string& text, std::uint32_t lo, std::uint32_t hi, std::uint32_t& out) {
    if (text.empty() || text.size() > 10 ||
        !std::all_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        return false;
    }
    const unsigned long long v = std::strtoull(text.c_str(), nullptr, 10);
    if (v < lo || v > hi) {
        return false;
    }
    out = static_cast<std::uint32_t>(v);
    return true;
}

}  // namespace

std::vector<Step> makePlan(const PlanOptions& opt) { return build(opt, nullptr); }

std::uint32_t roundCount(const PlanOptions& opt) {
    std::uint32_t n = 0;
    static_cast<void>(build(opt, &n));
    return n;
}

const char* keyName(std::uint16_t key) {
    switch (key) {
        case KEY_UP:
            return "KEY_UP";
        case KEY_DOWN:
            return "KEY_DOWN";
        case KEY_LEFT:
            return "KEY_LEFT";
        case KEY_RIGHT:
            return "KEY_RIGHT";
        case KEY_ENTER:
            return "KEY_ENTER";
        case KEY_ESC:
            return "KEY_ESC";
        case KEY_SPACE:
            return "KEY_SPACE";
        case KEY_P:
            return "KEY_P";
        case KEY_S:
            return "KEY_S";
        case KEY_K:
            return "KEY_K";
        case KEY_A:
            return "KEY_A";
        default:
            return "KEY_?";
    }
}

std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error) {
    Options opt;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--help" || a == "-h") {
            opt.help = true;
            continue;
        }
        if (a == "--dry-run") {
            opt.dryRun = true;
            continue;
        }
        if (a != "--minutes" && a != "--seed" && a != "--play" && a != "--device" &&
            a != "--delay") {
            error = "unknown argument '" + a + "'";
            return std::nullopt;
        }
        if (i + 1 >= args.size()) {
            error = a + " needs a value";
            return std::nullopt;
        }
        const std::string& v = args[++i];
        bool ok = true;
        if (a == "--minutes") {
            ok = parseUint(v, 1, 1440, opt.plan.minutes);
        } else if (a == "--seed") {
            ok = parseUint(v, 0, 0xFFFFFFFFu, opt.plan.seed);
        } else if (a == "--play") {
            ok = parseUint(v, 10, 3600, opt.plan.playSeconds);
        } else if (a == "--delay") {
            ok = parseUint(v, 0, 600, opt.delaySeconds);
        } else {
            ok = !v.empty();
            opt.device = v;
        }
        if (!ok) {
            error = "bad value for ";
            error += a;
            error += ": '";
            error += v;
            error += "'";
            return std::nullopt;
        }
    }
    return opt;
}

std::string usage() {
    return "usage: soak_driver [--minutes N] [--seed N] [--play N] [--device PATH]\n"
           "                   [--delay N] [--dry-run] [--help]\n"
           "  Plays Snake and Breakout in turn through a uinput keyboard for the QA-05 soak.\n"
           "  --minutes N  total length, default 120 (QA-05)\n"
           "  --seed N     key pattern seed, default 1\n"
           "  --play N     seconds of play per round, default 180\n"
           "  --device P   uinput device, default /dev/uinput\n"
           "  --delay N    seconds to start arcade after the keyboard appears, default 15\n"
           "  --dry-run    print the plan only; no device is opened\n";
}

}  // namespace arcade::soak
