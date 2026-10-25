// input_throughput_test: the PR-06 test (issue #23). Creates a virtual keyboard with uinput,
// runs the real Input Manager on it at the game's tick rate, injects 1,800 press/release pairs
// at 30 keys/s and checks that exactly 1,800 KeyDown and 1,800 KeyUp come back, in order.
//
// Needs write access to the uinput device (normally root). Run by hand on a Linux PC and on
// the board; see sw/tools/uinput/README.md. Prints PASS or FAIL as its last line and exits
// with 0 or 1.
#include <linux/input.h>
#include <sys/ioctl.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "input/evdev_sys.hpp"
#include "input/input_manager.hpp"
#include "tools/uinput/inject_plan.hpp"
#include "tools/uinput/options.hpp"
#include "tools/uinput/throughput_check.hpp"
#include "tools/uinput/uinput_device.hpp"

namespace {

using arcade::InputManager;
using arcade::KeyEvent;
using arcade::KeyEventType;
using Clock = std::chrono::steady_clock;
using std::chrono::nanoseconds;

/// Name of the virtual keyboard. Not an SRS value.
constexpr const char* kDeviceName = "arcade uinput inject";
/// App. A TICK_RATE (SR-FW-02): the Input Manager is polled once per game tick.
constexpr int kTickRateHz = 60;
/// How long to wait for the kernel (and udev) to create the device's /dev/input node, and how
/// often to retry. Not SRS values.
constexpr auto kStartTimeout = std::chrono::seconds(5);
constexpr auto kStartRetry = std::chrono::milliseconds(100);
/// Ticks keep running this long after the last key, so nothing still queued is missed. Not an
/// SRS value.
constexpr auto kDrainTime = std::chrono::milliseconds(500);
/// Gap between the Input Manager finding the device and the first key. Not an SRS value.
constexpr auto kLeadIn = std::chrono::milliseconds(100);

/// The real Linux calls, but only the injector's device counts as a keyboard, so the test never
/// selects or grabs the PC's own keyboard (which usually has a lower event number, SR-INP-01).
/// Wraps LinuxEvdevSys instead of deriving from it, so the production class stays final; every
/// other call is passed straight through.
class InjectorOnlySys final : public arcade::EvdevSys {
  public:
    std::vector<std::string> listDir(const std::string& dir) override { return real_.listDir(dir); }
    int open(const std::string& path, int flags) override { return real_.open(path, flags); }
    void close(int fd) override { real_.close(fd); }
    bool keyBits(int fd, arcade::KeyBits& bits) override {
        std::array<char, 256> name{};  // EVIOCGNAME fills at most size - 1 characters
        if (::ioctl(fd, EVIOCGNAME(name.size() - 1), name.data()) < 0 ||  // NOLINT
            std::strcmp(name.data(), kDeviceName) != 0) {
            return false;
        }
        return real_.keyBits(fd, bits);
    }
    bool grab(int fd) override { return real_.grab(fd); }
    ssize_t read(int fd, void* buf, std::size_t len, int& err) override {
        return real_.read(fd, buf, len, err);
    }

  private:
    arcade::LinuxEvdevSys real_;
};

int fail(const char* what, const std::string& detail) {
    std::printf("FAIL: %s %s\nFAIL\n", what, detail.c_str());
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    arcade::uinput::Options opt;
    std::string error;
    if (!arcade::uinput::parseOptions(argc, argv, opt, error)) {
        static_cast<void>(std::fprintf(stderr, "%s\n%s", error.c_str(),
                                       arcade::uinput::usage("input_throughput_test").c_str()));
        return 2;
    }
    if (opt.help) {
        std::printf("%s", arcade::uinput::usage("input_throughput_test").c_str());
        return 0;
    }

    // SR-INP-01: report every key the Input Manager requires, so it selects this device.
    const std::vector<std::uint16_t> keys(InputManager::kRequiredKeys.begin(),
                                          InputManager::kRequiredKeys.end());
    arcade::uinput::UinputDevice device;
    if (!device.create(opt.device, kDeviceName, keys)) {
        return fail("cannot create the virtual keyboard:", device.error());
    }

    InjectorOnlySys sys;
    InputManager im(sys);
    const Clock::time_point waitStart = Clock::now();
    while (!im.start()) {
        if (Clock::now() - waitStart > kStartTimeout) {
            return fail("the Input Manager did not find the virtual keyboard", "");
        }
        std::this_thread::sleep_for(kStartRetry);
    }
    const double planSeconds =
        std::chrono::duration<double>(arcade::uinput::planDuration(opt.plan)).count();
    std::printf("keyboard: %s\ninjecting %zu pairs at %u keys/s (%.1f s), polling at %d ticks/s\n",
                im.devicePath().c_str(), opt.plan.pairs, opt.plan.keysPerSecond, planSeconds,
                kTickRateHz);
    static_cast<void>(std::fflush(stdout));

    std::atomic<bool> injectDone{false};
    bool injectOk = true;
    const Clock::time_point start = Clock::now() + kLeadIn;
    std::thread injector([&] {
        injectOk = arcade::uinput::runPlan(
            opt.plan, [&](nanoseconds at) { std::this_thread::sleep_until(start + at); },
            [&](std::uint16_t code, std::int32_t value) { return device.sendKey(code, value); });
        injectDone = true;
    });

    arcade::uinput::ThroughputCheck check(opt.plan);
    const nanoseconds tick = nanoseconds(std::chrono::seconds(1)) / kTickRateHz;
    Clock::time_point next = Clock::now();
    bool stopping = false;
    Clock::time_point stopAt;
    std::size_t ticks = 0;
    std::size_t mostInOneTick = 0;
    for (;;) {
        std::size_t inThisTick = 0;
        im.poll([&](const KeyEvent& ev) {
            ++inThisTick;
            if (ev.type == KeyEventType::KeyDown) {
                check.keyDown(ev.code);
            } else if (ev.type == KeyEventType::KeyUp) {
                check.keyUp(ev.code);
            } else {
                check.otherEvent();
            }
        });
        ++ticks;
        mostInOneTick = std::max(mostInOneTick, inThisTick);
        if (injectDone && !stopping) {
            stopping = true;
            stopAt = Clock::now() + kDrainTime;
        }
        if (stopping && Clock::now() >= stopAt) {
            break;
        }
        next += tick;
        std::this_thread::sleep_until(next);
    }
    injector.join();

    std::printf("KeyDown: %zu of %zu\nKeyUp:   %zu of %zu\nerrors:  %zu\n", check.keyDowns(),
                opt.plan.pairs, check.keyUps(), opt.plan.pairs, check.errors());
    std::printf("ticks:   %zu, most events in one tick: %zu\n", ticks, mostInOneTick);
    if (!injectOk) {
        return fail("injecting failed:", device.error());
    }
    if (!check.firstError().empty()) {
        std::printf("FAIL: first error: %s\n", check.firstError().c_str());
    }
    // PR-06: every press and release reported, none lost.
    if (check.passed()) {
        std::printf("PASS\n");
        return 0;
    }
    std::printf("FAIL\n");
    return 1;
}
