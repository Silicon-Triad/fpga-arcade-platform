// soak_driver (QA-05, issue #51): plays Snake and Breakout in turn for the 2-hour soak by
// sending the makePlan() keys through a uinput virtual keyboard (sw/tools/uinput, #23). Logs
// every key with its time. Ctrl+C releases any held key and stops.
//
// Exit status: 0 plan finished (or --dry-run / --help), 1 device error or stopped, 2 bad
// arguments.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "tools/soak/soak_plan.hpp"
#include "tools/uinput/uinput_device.hpp"

namespace {

using arcade::soak::Step;
using Clock = std::chrono::steady_clock;

constexpr int kExitOk = 0;
constexpr int kExitError = 1;
constexpr int kExitUsage = 2;
constexpr const char* kDeviceName = "arcade-soak-keyboard";
// Sleep in slices so Ctrl+C is seen within this time. Not an SRS value.
constexpr std::chrono::milliseconds kSlice{200};

volatile std::sig_atomic_t gStop = 0;
extern "C" void onSignal(int /*sig*/) { gStop = 1; }

void printStep(const Step& s) {
    (void)std::printf("t=%10.3f s %-7s %-10s %s\n", static_cast<double>(s.atMs) / 1000.0,
                      s.value == 1 ? "press" : "release", arcade::soak::keyName(s.key), s.note);
}

/// Sleeps until `at`, in slices; false if a stop signal arrived.
bool sleepUntil(Clock::time_point at) {
    while (gStop == 0) {
        const auto now = Clock::now();
        if (now >= at) {
            return true;
        }
        std::this_thread::sleep_until(std::min(at, now + kSlice));
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::string error;
    const auto opt = arcade::soak::parseOptions(args, error);
    if (!opt) {
        (void)std::fprintf(stderr, "soak_driver: %s\n%s", error.c_str(),
                           arcade::soak::usage().c_str());
        return kExitUsage;
    }
    if (opt->help) {
        (void)std::printf("%s", arcade::soak::usage().c_str());
        return kExitOk;
    }
    const std::vector<Step> plan = arcade::soak::makePlan(opt->plan);
    (void)std::printf("soak_driver (QA-05): %u min, seed %u, %u rounds, %zu key events\n",
                      opt->plan.minutes, opt->plan.seed, arcade::soak::roundCount(opt->plan),
                      plan.size());
    if (opt->dryRun) {
        for (const Step& s : plan) {
            printStep(s);
        }
        return kExitOk;
    }

    (void)std::signal(SIGINT, &onSignal);
    (void)std::signal(SIGTERM, &onSignal);
    arcade::uinput::UinputDevice device;
    const std::vector<std::uint16_t> keys(arcade::soak::kSoakKeys.begin(),
                                          arcade::soak::kSoakKeys.end());
    if (!device.create(opt->device, kDeviceName, keys)) {
        (void)std::fprintf(stderr, "soak_driver: %s\n", device.error().c_str());
        return kExitError;
    }
    (void)std::printf("virtual keyboard '%s' created; start arcade now (%u s)\n", kDeviceName,
                      opt->delaySeconds);
    (void)std::fflush(stdout);
    if (!sleepUntil(Clock::now() + std::chrono::seconds(opt->delaySeconds))) {
        return kExitError;
    }

    const auto start = Clock::now();
    std::vector<std::uint16_t> held;
    bool ok = true;
    for (const Step& s : plan) {
        if (!sleepUntil(start + std::chrono::milliseconds(s.atMs))) {
            ok = false;
            break;
        }
        if (!device.sendKey(s.key, s.value)) {
            (void)std::fprintf(stderr, "soak_driver: %s\n", device.error().c_str());
            ok = false;
            break;
        }
        if (s.value == 1) {
            held.push_back(s.key);
        } else {
            held.erase(std::remove(held.begin(), held.end(), s.key), held.end());
        }
        printStep(s);
        (void)std::fflush(stdout);
    }
    for (const std::uint16_t key : held) {  // never leave a key pressed
        (void)device.sendKey(key, 0);
    }
    (void)std::printf("soak_driver: %s\n", ok ? "plan finished" : "stopped");
    return ok ? kExitOk : kExitError;
}
