// frame_rate (#46): measures the VGA frame rate by counting the Frame Counter register
// (§3.1.4, increments once per frame) against CLOCK_MONOTONIC, and gives the PR-01 verdict
// (59.7 Hz to 60.3 Hz). Replaces the scope / frequency counter of PR-01's verification,
// which the team does not have.
//
// Board only, as root. Uses DevMemRegisterSpace::open() (#34), which checks the FPGA manager,
// the bridge enable, ID / Version and Status FPGA ready before any access (SR-HAL-09).
// Reads only the Frame Counter afterwards; writes nothing.
//
// Exit status: 0 PASS, 1 FAIL, 2 bad arguments, 3 bridge not ready or not mappable.

#include <cerrno>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "hal/bridge_sys.hpp"
#include "hal/dev_mem_register_space.hpp"
#include "hal/frame_rate.hpp"
#include "hal/tilemap_hal.hpp"

namespace {

using namespace arcade::hal;
using namespace arcade::hal::fps;

constexpr int kExitPass = 0;
constexpr int kExitFail = 1;
constexpr int kExitUsage = 2;
constexpr int kExitNotReady = 3;

/// nanosleep for `d`, resumed after a signal interruption.
void sleepFor(std::chrono::nanoseconds d) {
    timespec req{};
    req.tv_sec = static_cast<time_t>(d.count() / 1000000000);
    req.tv_nsec = static_cast<long>(d.count() % 1000000000);
    timespec rem{};
    while (::nanosleep(&req, &rem) != 0 && errno == EINTR) {
        req = rem;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    std::string error;
    const auto opt = parseOptions(args, error);
    if (!opt) {
        std::cerr << "frame_rate: " << error << "\n" << usage();
        return kExitUsage;
    }
    if (opt->help) {
        std::cout << usage();
        return kExitPass;
    }

    LinuxBridgeSys sys;
    DevMemRegisterSpace regs(sys, kBoardReadiness);
    const BridgeResult open = regs.open();  // SR-HAL-09 then SR-HAL-01
    if (!open.ok()) {
        std::cerr << "frame_rate: " << describeBridgeFailure(open, regs.readiness())
                  << " Nothing read.\n";
        return kExitNotReady;
    }

    SteadyClock clock;  // std::chrono::steady_clock is CLOCK_MONOTONIC on Linux
    std::cout << "frame_rate (#46, PR-01): " << opt->seconds << " s, sample every " << opt->sample
              << " s, mode " << (opt->mode == Mode::kXga1024 ? "1024x768" : "640x480") << "\n";
    const Result r =
        measure(regs, clock, sleepFor, *opt, [](const Sample& s) { printSample(std::cout, s); });
    printReport(std::cout, r, opt->mode);
    regs.release();  // SR-HAL-07: unmap; writes nothing
    return r.passed() ? kExitPass : kExitFail;
}
