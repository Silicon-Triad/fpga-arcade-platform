// uinput_inject: creates a virtual keyboard with uinput and presses keys on it, 1,800
// press/release pairs at 30 keys/s by default (PR-06, issue #23). For manual tests of a running
// program, such as the arcade itself; the automatic PR-06 check is input_throughput_test.
//
// The keys go to whatever reads this virtual keyboard. If nothing has grabbed it, they reach
// the program with the keyboard focus, so it presses letters only (no Enter).
// Needs write access to the uinput device (normally root).
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

#include "input/input_manager.hpp"
#include "tools/uinput/inject_plan.hpp"
#include "tools/uinput/options.hpp"
#include "tools/uinput/uinput_device.hpp"

namespace {

/// Name of the virtual keyboard. Not an SRS value.
constexpr const char* kDeviceName = "arcade uinput inject";

}  // namespace

int main(int argc, char** argv) {
    arcade::uinput::Options opt;
    std::string error;
    if (!arcade::uinput::parseOptions(argc, argv, opt, error)) {
        static_cast<void>(std::fprintf(stderr, "%s\n%s", error.c_str(),
                                       arcade::uinput::usage("uinput_inject").c_str()));
        return 2;
    }
    if (opt.help) {
        std::printf("%s", arcade::uinput::usage("uinput_inject").c_str());
        return 0;
    }

    // SR-INP-01: report every key the Input Manager requires, so the arcade can select it.
    const std::vector<std::uint16_t> keys(arcade::InputManager::kRequiredKeys.begin(),
                                          arcade::InputManager::kRequiredKeys.end());
    arcade::uinput::UinputDevice device;
    if (!device.create(opt.device, kDeviceName, keys)) {
        static_cast<void>(std::fprintf(stderr, "cannot create the virtual keyboard: %s\n",
                                       device.error().c_str()));
        return 1;
    }
    const double planSeconds =
        std::chrono::duration<double>(arcade::uinput::planDuration(opt.plan)).count();
    std::printf(
        "virtual keyboard \"%s\" created.\n"
        "%zu pairs at %u keys/s (%.1f s) start in %u s. Keys go to whatever reads this keyboard.\n",
        kDeviceName, opt.plan.pairs, opt.plan.keysPerSecond, planSeconds, opt.delaySeconds);
    static_cast<void>(std::fflush(stdout));
    std::this_thread::sleep_for(std::chrono::seconds(opt.delaySeconds));

    using Clock = std::chrono::steady_clock;
    const Clock::time_point start = Clock::now();
    const bool ok = arcade::uinput::runPlan(
        opt.plan, [&](std::chrono::nanoseconds at) { std::this_thread::sleep_until(start + at); },
        [&](std::uint16_t code, std::int32_t value) { return device.sendKey(code, value); });
    if (!ok) {
        static_cast<void>(std::fprintf(stderr, "injecting failed: %s\n", device.error().c_str()));
        return 1;
    }
    std::printf("done: %zu pairs sent\n", opt.plan.pairs);
    return 0;
}
