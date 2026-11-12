// Unit tests for openCheckedBridge(), the body of openBridge() in main.cpp (issue #34;
// SR-HAL-09, SR-HAL-02, SF-02), on fake system calls.
#include "app/bridge_open.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "../backend_fpga/fake_evdev_sys.hpp"
#include "../hal/fake_bridge_sys.hpp"
#include "../hal/mock_register_space.hpp"
#include "app/fpga_start.hpp"
#include "app/options.hpp"
#include "app/startup.hpp"

namespace arcade {
namespace {

using hal::test::FakeBridgeSys;
using hal::test::FakeClock;
using hal::test::kTestReadiness;
using test::FakeEvdevSys;

struct BridgeOpenTest : ::testing::Test {
    FakeBridgeSys sys;
    hal::DevMemRegisterSpace space{sys, kTestReadiness};
    std::string stepText;
    FakeClock clock;
    FakeEvdevSys evdev;
    InputManager input{evdev};
    KeyBindings bindings;

    /// Runs the program with --backend=fpga on openCheckedBridge(); returns the exit status
    /// and stores the console output in `text`.
    int run(std::string& text) {
        std::FILE* log = std::tmpfile();
        if (log == nullptr) {
            return -1;
        }
        Options options;
        options.backend = BackendKind::Fpga;
        int checks = 0;
        const int status = runArcade(
            options,
            [this](const Options& /*options*/) {
                return startFpgaBackend(openCheckedBridge(space, stepText), clock, input, bindings);
            },
            log, [&checks] { return ++checks > 5; });
        (void)std::fseek(log, 0, SEEK_SET);
        text.assign(512, '\0');
        text.resize(std::fread(text.data(), 1, text.size(), log));
        (void)std::fclose(log);
        return status;
    }
};

TEST_F(BridgeOpenTest, SF_02_IdMismatchPrintsNotReadyWithTheIdStepAndExitsNonZero) {
    // Fresh boot: configured with the default design, ID/Version reads 0.
    std::string text;
    EXPECT_EQ(run(text), kExitHardwareNotReady);
    EXPECT_EQ(text,
              "graphics hardware not ready: check ID/Version register: reads 0x00000000, "
              "expected 0xA2CADE01 (our FPGA design is not loaded)\n");
    EXPECT_EQ(sys.count("munmap"), 1);               // the mapping was released
    EXPECT_EQ(input.state(), InputState::Scanning);  // keyboard left alone
}

TEST_F(BridgeOpenTest, SF_02_BridgeDisabledPrintsTheBridgeStepWithoutOpeningDevMem) {
    sys.files[std::string(kTestReadiness.bridgeEnablePath)] = {0, "0\n"};
    std::string text;
    EXPECT_EQ(run(text), kExitHardwareNotReady);
    EXPECT_EQ(text,
              "graphics hardware not ready: check lightweight bridge enabled: "
              "/fake/bridge/enable reads \"0\", expected \"1\"\n");
    EXPECT_EQ(sys.count("open"), 0);
    EXPECT_EQ(sys.count("mmap"), 0);
}

TEST_F(BridgeOpenTest, SR_HAL_09_AReadyBridgeIsHandedOverAsTheSpaceItself) {
    sys.loadOurDesign();
    const BridgeOpen bridge = openCheckedBridge(space, stepText);
    EXPECT_EQ(bridge.regs, &space);
    EXPECT_TRUE(stepText.empty());
}

TEST_F(BridgeOpenTest, SR_HAL_07_TheProgramRunsOnTheWindowAndReleasesItOnce) {
    sys.loadOurDesign();
    std::string text;
    EXPECT_EQ(run(text), 0);
    EXPECT_EQ(text, "");
    EXPECT_FALSE(space.isMapped());  // released by the backend's shutdown (SR-HAL-07)
    EXPECT_EQ(sys.count("munmap"), 1);
    EXPECT_EQ(sys.count("close"), 1);
}

}  // namespace
}  // namespace arcade
