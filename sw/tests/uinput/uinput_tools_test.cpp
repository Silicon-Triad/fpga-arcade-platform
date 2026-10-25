// Unit tests of the uinput test tools that need no uinput device (issue #23, PR-06): the
// injection plan, the throughput check and the command-line options. The full 60 s PR-06 run
// is input_throughput_test, run by hand (sw/tools/uinput/README.md).
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "input/input_manager.hpp"
#include "tools/uinput/inject_plan.hpp"
#include "tools/uinput/options.hpp"
#include "tools/uinput/throughput_check.hpp"

namespace arcade::uinput {
namespace {

using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;

struct Sent {
    nanoseconds at;
    std::uint16_t code;
    std::int32_t value;
};

/// Runs `plan` with a fake clock: waitUntil jumps straight to the requested time.
std::vector<Sent> runFake(const InjectPlan& plan, std::size_t failAt = SIZE_MAX) {
    std::vector<Sent> sent;
    nanoseconds now(0);
    runPlan(
        plan,
        [&](nanoseconds at) {
            EXPECT_GE(at, now);  // time never goes backwards
            now = at;
        },
        [&](std::uint16_t code, std::int32_t value) {
            if (sent.size() == failAt) {
                return false;
            }
            sent.push_back({now, code, value});
            return true;
        });
    return sent;
}

// --- Plan ---------------------------------------------------------------------------------------

TEST(InjectPlan, PR_06_DefaultsAre1800PairsAt30PerSecond) {
    const InjectPlan plan;
    EXPECT_EQ(plan.pairs, 1800U);
    EXPECT_EQ(plan.keysPerSecond, 30U);
}

TEST(InjectPlan, PR_06_SendsEveryPressAndReleaseInOrder) {
    const auto sent = runFake(InjectPlan{});
    ASSERT_EQ(sent.size(), 2U * 1800U);
    for (std::size_t i = 0; i < 1800; ++i) {
        EXPECT_EQ(sent[2 * i].value, 1) << i;
        EXPECT_EQ(sent[2 * i + 1].value, 0) << i;
        EXPECT_EQ(sent[2 * i].code, sent[2 * i + 1].code) << i;
        EXPECT_EQ(sent[2 * i].code, keyForPair(i)) << i;
    }
}

TEST(InjectPlan, PR_06_ThirtyPressesPerSecondFor60Seconds) {
    const InjectPlan plan;
    EXPECT_EQ(pressAt(plan, 0), nanoseconds(0));
    EXPECT_EQ(pressAt(plan, 30), seconds(1));
    EXPECT_EQ(pressAt(plan, 1799), nanoseconds(seconds(1799)) / 30);  // just under 60 s
    // Every second of the run holds exactly 30 presses.
    const auto sent = runFake(plan);
    std::vector<int> perSecond(60, 0);
    for (const Sent& s : sent) {
        if (s.value == 1) {
            ++perSecond.at(static_cast<std::size_t>(s.at / seconds(1)));
        }
    }
    for (std::size_t sec = 0; sec < perSecond.size(); ++sec) {
        EXPECT_EQ(perSecond[sec], 30) << "second " << sec;
    }
}

TEST(InjectPlan, PR_06_EachKeyIsReleasedBeforeTheNextPress) {
    const InjectPlan plan;
    for (std::size_t i = 0; i + 1 < plan.pairs; ++i) {
        EXPECT_GT(releaseAt(plan, i), pressAt(plan, i)) << i;
        EXPECT_LT(releaseAt(plan, i), pressAt(plan, i + 1)) << i;
    }
}

TEST(InjectPlan, PR_06_DurationIsJustUnder60Seconds) {
    const InjectPlan plan;
    EXPECT_GT(planDuration(plan), seconds(59));
    EXPECT_LT(planDuration(plan), seconds(60));
    EXPECT_EQ(planDuration(InjectPlan{0, 30}), nanoseconds(0));
}

TEST(InjectPlan, PR_06_LettersOnlyInTurn) {
    EXPECT_EQ(keyForPair(0), KEY_A);
    EXPECT_EQ(keyForPair(25), KEY_Z);
    EXPECT_EQ(keyForPair(26), KEY_A);
    // The Input Manager would select a device reporting these keys (SR-INP-01).
    for (const std::uint16_t key : kInjectKeys) {
        bool required = false;
        for (const std::uint16_t r : InputManager::kRequiredKeys) {
            required = required || (r == key);
        }
        EXPECT_TRUE(required) << key;
    }
}

TEST(InjectPlan, PR_06_StopsAtTheFirstFailedSend) {
    const auto sent = runFake(InjectPlan{10, 30}, 3);
    EXPECT_EQ(sent.size(), 3U);
    std::size_t calls = 0;
    const bool ok = runPlan(
        InjectPlan{10, 30}, [](nanoseconds) {},
        [&](std::uint16_t, std::int32_t) { return ++calls < 3; });
    EXPECT_FALSE(ok);
    EXPECT_EQ(calls, 3U);
}

// --- Throughput check ---------------------------------------------------------------------------

/// Feeds `check` the events a perfect Input Manager would report for `plan`.
void feedPerfect(ThroughputCheck& check, const InjectPlan& plan) {
    for (std::size_t i = 0; i < plan.pairs; ++i) {
        check.keyDown(keyForPair(i));
        check.keyUp(keyForPair(i));
    }
}

TEST(ThroughputCheck, PR_06_AllPairsInOrderPass) {
    const InjectPlan plan;
    ThroughputCheck check(plan);
    feedPerfect(check, plan);
    EXPECT_TRUE(check.passed());
    EXPECT_EQ(check.keyDowns(), 1800U);
    EXPECT_EQ(check.keyUps(), 1800U);
    EXPECT_TRUE(check.firstError().empty());
}

TEST(ThroughputCheck, PR_06_OneLostReleaseFails) {
    const InjectPlan plan{3, 30};
    ThroughputCheck check(plan);
    check.keyDown(keyForPair(0));
    check.keyUp(keyForPair(0));
    check.keyDown(keyForPair(1));  // its KeyUp is lost
    check.keyDown(keyForPair(2));
    check.keyUp(keyForPair(2));
    EXPECT_FALSE(check.passed());
    EXPECT_EQ(check.keyUps(), 2U);
    EXPECT_NE(check.firstError().find("before the KeyUp of pair 1"), std::string::npos);
}

TEST(ThroughputCheck, PR_06_LostPressFails) {
    const InjectPlan plan{2, 30};
    ThroughputCheck check(plan);
    check.keyUp(keyForPair(0));  // its KeyDown is lost
    check.keyDown(keyForPair(1));
    check.keyUp(keyForPair(1));
    EXPECT_FALSE(check.passed());
    EXPECT_NE(check.firstError().find("without its KeyDown"), std::string::npos);
}

TEST(ThroughputCheck, PR_06_TooFewPairsFail) {
    const InjectPlan plan{5, 30};
    ThroughputCheck check(plan);
    feedPerfect(check, InjectPlan{4, 30});
    EXPECT_FALSE(check.passed());
    EXPECT_EQ(check.errors(), 0U);  // nothing wrong, just missing at the end
}

TEST(ThroughputCheck, PR_06_ExtraEventsFail) {
    const InjectPlan plan{2, 30};
    ThroughputCheck check(plan);
    feedPerfect(check, plan);
    check.keyDown(KEY_A);
    EXPECT_FALSE(check.passed());
    EXPECT_NE(check.firstError().find("after the last pair"), std::string::npos);
}

TEST(ThroughputCheck, PR_06_WrongKeyFails) {
    const InjectPlan plan{1, 30};
    ThroughputCheck check(plan);
    check.keyDown(KEY_B);
    check.keyUp(KEY_B);
    EXPECT_FALSE(check.passed());
    EXPECT_NE(check.firstError().find("expected"), std::string::npos);
}

TEST(ThroughputCheck, PR_06_OtherEventsFail) {
    const InjectPlan plan{1, 30};
    ThroughputCheck check(plan);
    check.keyDown(keyForPair(0));
    check.otherEvent();  // e.g. KeyboardLost during the run
    check.keyUp(keyForPair(0));
    EXPECT_FALSE(check.passed());
    EXPECT_EQ(check.errors(), 1U);
}

// --- Options ------------------------------------------------------------------------------------

bool parse(std::vector<const char*> args, Options& out, std::string& error) {
    args.insert(args.begin(), "prog");
    return parseOptions(static_cast<int>(args.size()), args.data(), out, error);
}

TEST(UinputOptions, PR_06_DefaultsWithoutArguments) {
    Options o;
    std::string e;
    ASSERT_TRUE(parse({}, o, e));
    EXPECT_EQ(o.plan.pairs, kPr06Pairs);
    EXPECT_EQ(o.plan.keysPerSecond, kPr06KeysPerSecond);
    EXPECT_EQ(o.device, "/dev/uinput");
    EXPECT_FALSE(o.help);
}

TEST(UinputOptions, AllOptionsAreRead) {
    Options o;
    std::string e;
    ASSERT_TRUE(
        parse({"--pairs", "60", "--rate", "100", "--device", "/tmp/u", "--delay", "0"}, o, e))
        << e;
    EXPECT_EQ(o.plan.pairs, 60U);
    EXPECT_EQ(o.plan.keysPerSecond, 100U);
    EXPECT_EQ(o.device, "/tmp/u");
    EXPECT_EQ(o.delaySeconds, 0U);
    Options h;
    ASSERT_TRUE(parse({"--help"}, h, e));
    EXPECT_TRUE(h.help);
}

TEST(UinputOptions, BadValuesAreRejected) {
    const std::vector<std::vector<const char*>> bad = {
        {"--rate", "0"},    {"--rate", "-5"},  {"--pairs", "abc"},
        {"--pairs", "12x"}, {"--pairs", ""},   {"--pairs"},
        {"--frobnicate"},   {"--delay", "+1"}, {"--rate", "99999999999999999999"}};
    for (const auto& args : bad) {
        Options o;
        std::string e;
        EXPECT_FALSE(parse(args, o, e)) << args[0];
        EXPECT_FALSE(e.empty()) << args[0];
    }
}

TEST(UinputOptions, UsageNamesTheDefaults) {
    const std::string u = usage("uinput_inject");
    EXPECT_NE(u.find("1800"), std::string::npos);
    EXPECT_NE(u.find("30"), std::string::npos);
    EXPECT_NE(u.find("/dev/uinput"), std::string::npos);
}

}  // namespace
}  // namespace arcade::uinput
