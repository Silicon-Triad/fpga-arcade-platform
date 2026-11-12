// Unit tests for the FPGA backend's start-up (issue #37, SR-FW-01, SR-FW-05, SF-02).
#include "app/fpga_start.hpp"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "../backend_fpga/fake_evdev_sys.hpp"
#include "../hal/mock_register_space.hpp"
#include "app/options.hpp"
#include "app/startup.hpp"

namespace arcade {
namespace {

using hal::test::FakeClock;
using hal::test::MockRegisterSpace;
using test::FakeEvdevSys;

struct FpgaStartTest : ::testing::Test {
    MockRegisterSpace regs;
    FakeClock clock;
    FakeEvdevSys evdev;
    InputManager input{evdev};
    KeyBindings bindings;
};

TEST_F(FpgaStartTest, SF_02_NoBridgeFailsWithItsStepAndLeavesTheKeyboardAlone) {
    const BackendStart start =
        startFpgaBackend(BridgeOpen{nullptr, "map bridge"}, clock, input, bindings);
    EXPECT_EQ(start.api, nullptr);
    EXPECT_EQ(start.failedStep, "map bridge");
    EXPECT_EQ(input.state(), InputState::Scanning);  // the Input Manager was not started
    EXPECT_TRUE(regs.log.empty());
}

TEST_F(FpgaStartTest, SF_02_RunArcadePrintsTheMissingStep) {
    std::FILE* log = std::tmpfile();
    ASSERT_NE(log, nullptr);
    Options options;
    options.backend = BackendKind::Fpga;
    const int status = runArcade(
        options,
        [this](const Options& /*options*/) {
            return startFpgaBackend(BridgeOpen{nullptr, "map bridge (#34)"}, clock, input,
                                    bindings);
        },
        log);
    EXPECT_EQ(status, kExitHardwareNotReady);
    (void)std::fseek(log, 0, SEEK_SET);
    std::string text(256, '\0');
    text.resize(std::fread(text.data(), 1, text.size(), log));
    EXPECT_EQ(text, "graphics hardware not ready: map bridge (#34)\n");
    (void)std::fclose(log);
}

TEST_F(FpgaStartTest, SR_FW_05_ABridgeGivesAnFpgaBackendWithTheKeyboard) {
    BackendStart start = startFpgaBackend(BridgeOpen{&regs, {}}, clock, input, bindings);
    ASSERT_NE(start.api, nullptr);
    ASSERT_TRUE(start.keyboardPresent);
    EXPECT_TRUE(start.keyboardPresent());  // SR-INP-01: the Input Manager was started
    EXPECT_EQ(start.finalFrameTicks, 0);   // the board keeps the last frame (SR-HAL-07)
    EXPECT_TRUE(regs.log.empty());         // no bridge access until loadTiles()
    start.api.reset();
    EXPECT_EQ(regs.releaseCalls, 1);  // SR-HAL-07
}

TEST_F(FpgaStartTest, SR_INP_16_NoKeyboardIsReportedNotFailed) {
    evdev.present = false;
    const BackendStart start = startFpgaBackend(BridgeOpen{&regs, {}}, clock, input, bindings);
    ASSERT_NE(start.api, nullptr);
    EXPECT_FALSE(start.keyboardPresent());
}

TEST_F(FpgaStartTest, SR_FW_01_RunArcadeLoadsTilesAndReleasesTheBridgeOnce) {
    Options options;
    options.backend = BackendKind::Fpga;
    int checks = 0;
    std::FILE* log = std::tmpfile();
    ASSERT_NE(log, nullptr);
    const int status = runArcade(
        options,
        [this](const Options& /*options*/) {
            return startFpgaBackend(BridgeOpen{&regs, {}}, clock, input, bindings);
        },
        log, [&checks] { return ++checks > 20; });  // a few ticks, then SR-FW-14's stop
    (void)std::fclose(log);
    EXPECT_EQ(status, 0);
    EXPECT_NE(regs.control, 0U);  // SR-HAL-13: Display Enable set, and never cleared
    EXPECT_EQ(regs.releaseCalls, 1);
    EXPECT_EQ(regs.accessesAfterRelease, 0);
}

}  // namespace
}  // namespace arcade
