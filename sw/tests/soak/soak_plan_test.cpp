#include "tools/soak/soak_plan.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "input/input_manager.hpp"
#include "input/key_bindings.hpp"

namespace arcade::soak {
namespace {

PlanOptions shortPlan(std::uint32_t minutes) {
    PlanOptions o;
    o.minutes = minutes;
    return o;
}

bool noteIs(const Step& s, const char* prefix) {
    return std::strncmp(s.note, prefix, std::strlen(prefix)) == 0;
}

}  // namespace

// QA-05: the default plan covers 2 hours and does not run past them.
TEST(SoakPlan, QA_05_DefaultIsTwoHoursAndFits) {
    const PlanOptions opt;
    EXPECT_EQ(opt.minutes, 120u);
    const auto plan = makePlan(opt);
    ASSERT_FALSE(plan.empty());
    EXPECT_LE(plan.back().atMs, 120ull * 60 * 1000);
    EXPECT_GT(plan.back().atMs, 110ull * 60 * 1000);  // no long idle tail
    EXPECT_GE(roundCount(opt), 20u);
}

TEST(SoakPlan, TimesAreOrderedAndEveryPressIsReleasedFirst) {
    const auto plan = makePlan(shortPlan(30));
    std::uint16_t held = 0;
    std::uint64_t last = 0;
    for (const Step& s : plan) {
        ASSERT_GE(s.atMs, last);
        last = s.atMs;
        if (s.value == 1) {
            ASSERT_EQ(held, 0) << "two keys held at " << s.atMs;
            held = s.key;
        } else {
            ASSERT_EQ(s.value, 0);
            ASSERT_EQ(held, s.key);
            held = 0;
        }
    }
    EXPECT_EQ(held, 0);
}

TEST(SoakPlan, SameSeedSamePlanOtherSeedOtherPlan) {
    PlanOptions a = shortPlan(10);
    const auto p1 = makePlan(a);
    const auto p2 = makePlan(a);
    ASSERT_EQ(p1.size(), p2.size());
    for (std::size_t i = 0; i < p1.size(); ++i) {
        ASSERT_EQ(p1[i].atMs, p2[i].atMs);
        ASSERT_EQ(p1[i].key, p2[i].key);
    }
    a.seed = 2;
    const auto p3 = makePlan(a);
    bool differs = p3.size() != p1.size();
    for (std::size_t i = 0; !differs && i < p1.size(); ++i) {
        differs = p1[i].key != p3[i].key || p1[i].atMs != p3[i].atMs;
    }
    EXPECT_TRUE(differs);
}

// QA-05 "alternating both games": replay the menu (3 entries, wraps, SR-UI-01) and check the
// game each Enter starts.
TEST(SoakPlan, QA_05_AlternatesSnakeAndBreakout) {
    const auto plan = makePlan(shortPlan(60));
    std::uint32_t selection = 0;
    std::vector<std::uint32_t> started;
    for (const Step& s : plan) {
        if (s.value != 1 || !noteIs(s, "menu")) {
            continue;
        }
        if (s.key == KEY_DOWN) {
            selection = (selection + 1) % kMenuEntries;
        } else if (s.key == KEY_ENTER) {
            started.push_back(selection);
        }
    }
    ASSERT_EQ(started.size(), roundCount(shortPlan(60)));
    ASSERT_GE(started.size(), 4u);
    for (std::size_t i = 0; i < started.size(); ++i) {
        EXPECT_EQ(started[i], i % 2) << "round " << i;
    }
}

// SR-UI-14: Esc held SHUTDOWN_HOLD (3 s) in the menu opens the shutdown prompt; M mutes.
TEST(SoakPlan, NeverHoldsEscAndNeverPressesMute) {
    const auto plan = makePlan(shortPlan(60));
    for (std::size_t i = 0; i + 1 < plan.size(); ++i) {
        ASSERT_NE(plan[i].key, KEY_M);
        if (plan[i].key == KEY_ESC && plan[i].value == 1) {
            ASSERT_EQ(plan[i + 1].key, KEY_ESC);
            EXPECT_LT(plan[i + 1].atMs - plan[i].atMs, 1000u);
        }
    }
}

// SR-UI-10: the Initials screen has no Back, so every round types three letters and Enter
// before its Esc (review of #131).
TEST(SoakPlan, InitialsInEveryRound) {
    const PlanOptions o = shortPlan(60);
    const auto plan = makePlan(o);
    const auto enters = std::count_if(plan.begin(), plan.end(), [](const Step& s) {
        return s.value == 1 && noteIs(s, "initials enter");
    });
    EXPECT_EQ(static_cast<std::uint32_t>(enters), roundCount(o));
}

namespace {

/// The screens a round can be on when its end keys start (section 3.1.1), and where they lead.
enum class Screen : std::uint8_t { Menu, Playing, Paused, GameOver, Initials, HighScores };

/// Applies one KeyDown to a model of the App screen flow, using the real TBL-KeyBinding
/// lookup: Back leaves Playing, Paused, GameOver and HighScores for the menu (SR-UI-07,
/// SR-UI-09, SR-UI-12); Pause toggles Playing and Paused (SR-UI-05, SR-UI-06); Select on
/// GameOver starts a new game; on Initials, Char adds a letter (at most 3) and Select with three
/// letters saves and opens HighScores (SR-UI-10). Initials has no Back.
void press(const KeyBindings& kb, Screen& screen, int& letters, std::uint16_t key) {
    const auto ctx = [&]() {
        switch (screen) {
            case Screen::Menu:
                return InputContext::Menu;
            case Screen::Playing:
                return InputContext::Snake;
            case Screen::Paused:
                return InputContext::Paused;
            case Screen::GameOver:
                return InputContext::GameOver;
            case Screen::Initials:
                return InputContext::Initials;
            case Screen::HighScores:
                return InputContext::HighScores;
        }
        return InputContext::Global;
    }();
    const auto action = kb.lookup(ctx, key);
    if (!action) {
        return;
    }
    switch (screen) {
        case Screen::Playing:
            if (*action == Action::Back) {
                screen = Screen::Menu;
            }
            if (*action == Action::Pause) {
                screen = Screen::Paused;
            }
            break;
        case Screen::Paused:
            if (*action == Action::Back) {
                screen = Screen::Menu;
            }
            if (*action == Action::Pause) {
                screen = Screen::Playing;
            }
            break;
        case Screen::GameOver:
            if (*action == Action::Back) {
                screen = Screen::Menu;
            }
            if (*action == Action::Select) {
                screen = Screen::Playing;
            }
            break;
        case Screen::Initials:
            if (*action == Action::Char && letters < 3) {
                ++letters;
            }
            if (*action == Action::Select && letters == 3) {
                screen = Screen::HighScores;
            }
            break;
        case Screen::HighScores:
            if (*action == Action::Back) {
                screen = Screen::Menu;
            }
            break;
        case Screen::Menu:
            break;
    }
}

}  // namespace

// Review of #131: from every screen a round can end on, including Initials with no letters
// typed, the round's end keys (initials, Enter, Esc) bring the arcade back to the menu, in
// every round of the plan.
TEST(SoakPlan, QA_05_EveryRoundEndReturnsToTheMenu) {
    const KeyBindings kb;
    const auto plan = makePlan(shortPlan(60));
    std::vector<std::vector<std::uint16_t>> ends(1);
    for (const Step& s : plan) {
        if (s.value != 1) {
            continue;
        }
        if (noteIs(s, "initials") || noteIs(s, "back to menu")) {
            ends.back().push_back(s.key);
        }
        if (noteIs(s, "back to menu")) {
            ends.emplace_back();
        }
    }
    ends.pop_back();  // empty after the last round
    ASSERT_EQ(ends.size(), roundCount(shortPlan(60)));
    for (std::size_t r = 0; r < ends.size(); ++r) {
        for (const Screen start : {Screen::Playing, Screen::Paused, Screen::GameOver,
                                   Screen::Initials, Screen::HighScores}) {
            Screen screen = start;
            int letters = 0;
            for (const std::uint16_t key : ends[r]) {
                press(kb, screen, letters, key);
            }
            EXPECT_EQ(screen, Screen::Menu)
                << "round " << r << " from screen " << static_cast<int>(start);
        }
    }
}

TEST(SoakPlan, PlaysPauseAndLaunch) {
    const auto plan = makePlan(shortPlan(30));
    std::set<std::string> notes;
    for (const Step& s : plan) {
        notes.insert(s.note);
    }
    for (const char* n : {"snake play", "snake pause", "snake resume", "breakout launch",
                          "breakout play", "breakout pause", "breakout end", "back to menu"}) {
        EXPECT_EQ(notes.count(n), 1u) << n;
    }
}

// SR-INP-01: the virtual keyboard must report every key the Input Manager requires, and every
// key the plan presses.
TEST(SoakPlan, SR_INP_01_VirtualKeyboardReportsTheRequiredKeys) {
    for (const std::uint16_t k : InputManager::kRequiredKeys) {
        EXPECT_NE(std::find(kSoakKeys.begin(), kSoakKeys.end(), k), kSoakKeys.end()) << k;
    }
    for (const Step& s : makePlan(shortPlan(30))) {
        ASSERT_NE(std::find(kSoakKeys.begin(), kSoakKeys.end(), s.key), kSoakKeys.end());
        EXPECT_STRNE(keyName(s.key), "KEY_?");
    }
}

// TBL-KeyBinding: each planned key does what the plan means in its screen's context.
TEST(SoakPlan, TBL_KeyBinding_PlannedKeysMapToTheIntendedActions) {
    const KeyBindings kb;
    struct Want {
        const char* note;
        InputContext context;
    };
    const std::vector<Want> wants = {
        {"menu down", InputContext::Menu},         {"menu start", InputContext::Menu},
        {"snake play", InputContext::Snake},       {"snake pause", InputContext::Snake},
        {"breakout play", InputContext::Breakout}, {"breakout launch", InputContext::Breakout},
        {"breakout end", InputContext::Breakout},  {"breakout pause", InputContext::Breakout},
        {"initials", InputContext::Initials},      {"back to menu", InputContext::Snake},
        {"back to menu", InputContext::Breakout},  {"back to menu", InputContext::GameOver},
    };
    for (const Step& s : makePlan(shortPlan(30))) {
        if (s.value != 1) {
            continue;
        }
        for (const Want& w : wants) {
            if (noteIs(s, w.note)) {
                EXPECT_TRUE(kb.lookup(w.context, s.key).has_value())
                    << s.note << " " << keyName(s.key);
            }
        }
    }
    EXPECT_EQ(kb.lookup(InputContext::Breakout, KEY_SPACE), Action::Launch);
    EXPECT_EQ(kb.lookup(InputContext::Initials, KEY_ENTER), Action::Select);
    EXPECT_EQ(kb.lookup(InputContext::Initials, KEY_S), Action::Char);
    // Back reaches the menu from every screen a round can end on.
    for (const InputContext c : {InputContext::Snake, InputContext::Breakout, InputContext::Paused,
                                 InputContext::GameOver, InputContext::HighScores}) {
        EXPECT_EQ(kb.lookup(c, KEY_ESC), Action::Back);
    }
    // The initials letters do nothing on GameOver (only Global M would).
    for (const int k : {KEY_S, KEY_K, KEY_A}) {
        EXPECT_FALSE(kb.lookup(InputContext::GameOver, static_cast<std::uint16_t>(k)).has_value());
    }
}

TEST(SoakPlan, OptionsDefaultsAndValues) {
    std::string err;
    const Options d = parseOptions({}, err).value_or(Options{});
    EXPECT_EQ(d.plan.minutes, 120u);
    EXPECT_EQ(d.device, "/dev/uinput");
    EXPECT_FALSE(d.dryRun);
    const Options o = parseOptions({"--minutes", "2", "--seed", "7", "--play", "30", "--device",
                                    "/tmp/u", "--delay", "0", "--dry-run", "--help"},
                                   err)
                          .value_or(Options{});
    EXPECT_EQ(o.plan.minutes, 2u);
    EXPECT_EQ(o.plan.seed, 7u);
    EXPECT_EQ(o.plan.playSeconds, 30u);
    EXPECT_EQ(o.device, "/tmp/u");
    EXPECT_EQ(o.delaySeconds, 0u);
    EXPECT_TRUE(o.dryRun);
    EXPECT_TRUE(o.help);
    EXPECT_NE(usage().find("--minutes"), std::string::npos);
}

TEST(SoakPlan, OptionsRejectBadInput) {
    std::string err;
    for (const auto& args : std::vector<std::vector<std::string>>{{"--minutes", "0"},
                                                                  {"--minutes", "1441"},
                                                                  {"--play", "5"},
                                                                  {"--delay", "601"},
                                                                  {"--seed", "x"},
                                                                  {"--device", ""},
                                                                  {"--minutes"},
                                                                  {"--bogus"}}) {
        err.clear();
        EXPECT_FALSE(parseOptions(args, err).has_value()) << args[0];
        EXPECT_FALSE(err.empty());
    }
}

}  // namespace arcade::soak
