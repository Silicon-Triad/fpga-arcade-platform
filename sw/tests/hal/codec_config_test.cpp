#include "hal/codec_config.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <string>
#include <vector>

#include "fake_codec_sys.hpp"
#include "hal/register_map.hpp"
#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using arcade::hal::test::Access;
using arcade::hal::test::FakeCodecSys;
using arcade::hal::test::kTestCodecPaths;
using arcade::hal::test::MockRegisterSpace;

constexpr std::uint32_t kAudioEnableMask = 1U << kBitAudioEnable;
constexpr std::uint32_t kMuteMask = 1U << kBitAudioMute;

bool called(const FakeCodecSys& sys, const std::string& call) {
    return std::find(sys.calls.begin(), sys.calls.end(), call) != sys.calls.end();
}

std::size_t bridgeWrites(const MockRegisterSpace& regs) {
    return static_cast<std::size_t>(std::count_if(regs.log.begin(), regs.log.end(), [](auto& a) {
        return a.kind == Access::Kind::kWrite32 || a.kind == Access::Kind::kWrite8;
    }));
}

/// The GPIO is back on the FPGA side and unexported after the call (step 4).
void expectBusReleased(const FakeCodecSys& sys) {
    EXPECT_TRUE(called(sys, "write /fake/gpio/gpio502/value 0"));
    EXPECT_TRUE(called(sys, "write /fake/gpio/unexport 502"));
}

}  // namespace

// §3.1.2.4: 7-bit register address, then 9 data bits; bit 8 rides in bit 0 of the first byte.
TEST(CodecConfig, SR_HAL_10_EncodesRegisterAndNineDataBits) {
    EXPECT_EQ(encodeCodecWrite({15, 0x000}), (std::array<std::uint8_t, 2>{0x1E, 0x00}));
    EXPECT_EQ(encodeCodecWrite({6, 0x067}), (std::array<std::uint8_t, 2>{0x0C, 0x67}));
    EXPECT_EQ(encodeCodecWrite({4, 0x012}), (std::array<std::uint8_t, 2>{0x08, 0x12}));
    EXPECT_EQ(encodeCodecWrite({7, 0x1FF}), (std::array<std::uint8_t, 2>{0x0F, 0xFF}));
    EXPECT_EQ(encodeCodecWrite({9, 0x101}), (std::array<std::uint8_t, 2>{0x13, 0x01}));
}

// §3.1.2.4 registers used, datasheet Table 30 values: reset first, ACTIVE last.
TEST(CodecConfig, SR_HAL_10_SetupOrderAndValues) {
    const std::array<std::uint8_t, 7> regs{15, 6, 4, 5, 7, 8, 9};
    const std::array<std::uint16_t, 7> data{0x000, 0x067, 0x012, 0x000, 0x002, 0x000, 0x001};
    for (std::size_t i = 0; i < kCodecSetup.size(); ++i) {
        EXPECT_EQ(kCodecSetup[i].reg, regs[i]) << i;
        EXPECT_EQ(kCodecSetup[i].data, data[i]) << i;
    }
    EXPECT_EQ(kCodecI2cAddress, 0x1A);  // §3.1.2.4 device address
    EXPECT_EQ(kHpsI2cControlBit, 19U);  // §3.1.2.4 bit 19 of GPIO1
}

TEST(CodecConfig, SR_HAL_10_ConfiguresCodecThenSetsAudioEnable) {
    FakeCodecSys sys;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    ASSERT_TRUE(r.ok()) << describeCodecFailure(r, kTestCodecPaths);
    EXPECT_EQ(r.gpio, 502);  // base 483 + bit 19, as measured on the board

    const std::vector<std::string> expected = {
        "list /fake/gpio",
        "read /fake/gpio/gpiochip483/label",
        "read /fake/gpio/gpiochip483/base",
        "read /fake/gpio/gpiochip483/ngpio",
        "write /fake/gpio/export 502",
        "write /fake/gpio/gpio502/direction out",
        "write /fake/gpio/gpio502/value 1",
        "open /fake/i2c-0",
        "addr 7 26",
        "i2c 7",
        "i2c 7",
        "i2c 7",
        "i2c 7",
        "i2c 7",
        "i2c 7",
        "i2c 7",
        "close 7",
        "write /fake/gpio/gpio502/value 0",
        "write /fake/gpio/unexport 502",
    };
    EXPECT_EQ(sys.calls, expected);

    const std::vector<std::vector<std::uint8_t>> bytes = {{0x1E, 0x00}, {0x0C, 0x67}, {0x08, 0x12},
                                                          {0x0A, 0x00}, {0x0E, 0x02}, {0x10, 0x00},
                                                          {0x12, 0x01}};
    EXPECT_EQ(sys.i2cWrites, bytes);

    // SR-HAL-10: Audio Enable set after the codec writes, with one read and one write.
    EXPECT_EQ(regs.audioControl, kAudioEnableMask);
    ASSERT_EQ(regs.log.size(), 2U);
    EXPECT_EQ(regs.log[0].kind, Access::Kind::kRead32);
    EXPECT_EQ(regs.log[0].offset, kOffsetAudioControl);
    EXPECT_EQ(regs.log[1].kind, Access::Kind::kWrite32);
    EXPECT_EQ(regs.log[1].offset, kOffsetAudioControl);
}

// SR-HAL-12 belongs to #44: setting Audio Enable keeps the mute bit as it was.
TEST(CodecConfig, SR_HAL_10_KeepsMuteBit) {
    FakeCodecSys sys;
    MockRegisterSpace regs;
    regs.audioControl = kMuteMask;
    ASSERT_TRUE(configureCodec(sys, regs, kTestCodecPaths).ok());
    EXPECT_EQ(regs.audioControl, kMuteMask | kAudioEnableMask);
}

// Exporting a GPIO that is already exported returns EBUSY; that is fine (seen on the board).
TEST(CodecConfig, SR_HAL_10_AlreadyExportedGpioIsFine) {
    FakeCodecSys sys;
    sys.writeErrors["/fake/gpio/export"] = EBUSY;
    MockRegisterSpace regs;
    EXPECT_TRUE(configureCodec(sys, regs, kTestCodecPaths).ok());
}

TEST(CodecConfig, SR_HAL_10_NoGpioChipTouchesNothing) {
    FakeCodecSys sys;
    sys.files["/fake/gpio/gpiochip483/label"] = "ff708000.gpio\n";  // another controller
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kGpioChipNotFound);
    EXPECT_FALSE(called(sys, "write /fake/gpio/export 502"));
    EXPECT_FALSE(called(sys, "open /fake/i2c-0"));
    EXPECT_TRUE(regs.log.empty());
    EXPECT_NE(describeCodecFailure(r, kTestCodecPaths).find("ff709000.gpio"), std::string::npos);
}

TEST(CodecConfig, SR_HAL_10_GpioChipTooSmallIsNotUsed) {
    FakeCodecSys sys;
    sys.files["/fake/gpio/gpiochip483/ngpio"] = "19\n";  // no line 19
    MockRegisterSpace regs;
    EXPECT_EQ(configureCodec(sys, regs, kTestCodecPaths).status, CodecStatus::kGpioChipNotFound);
    EXPECT_TRUE(regs.log.empty());
}

TEST(CodecConfig, SR_HAL_10_UnreadableGpioDirectoryIsNotFound) {
    FakeCodecSys sys;
    sys.dirs.clear();
    MockRegisterSpace regs;
    EXPECT_EQ(configureCodec(sys, regs, kTestCodecPaths).status, CodecStatus::kGpioChipNotFound);
}

TEST(CodecConfig, SR_HAL_10_ExportFailureStopsBeforeI2c) {
    FakeCodecSys sys;
    sys.writeErrors["/fake/gpio/export"] = EACCES;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kGpioSetupFailed);
    EXPECT_EQ(r.sysError, EACCES);
    EXPECT_FALSE(called(sys, "open /fake/i2c-0"));
    EXPECT_TRUE(regs.log.empty());
}

TEST(CodecConfig, SR_HAL_10_DriveHighFailureReleasesGpio) {
    FakeCodecSys sys;
    sys.writeErrors["/fake/gpio/gpio502/direction"] = EIO;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kGpioSetupFailed);
    EXPECT_FALSE(called(sys, "open /fake/i2c-0"));
    expectBusReleased(sys);
    EXPECT_TRUE(regs.log.empty());
}

TEST(CodecConfig, SR_HAL_10_I2cOpenFailureReleasesGpioAndStaysSilent) {
    FakeCodecSys sys;
    sys.openError = ENOENT;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kI2cOpenFailed);
    EXPECT_EQ(r.sysError, ENOENT);
    EXPECT_TRUE(sys.i2cWrites.empty());
    expectBusReleased(sys);
    EXPECT_EQ(bridgeWrites(regs), 0U);  // SR-AUD-09: Audio Enable never set
}

TEST(CodecConfig, SR_HAL_10_AddressFailureClosesAndReleases) {
    FakeCodecSys sys;
    sys.addressError = EBUSY;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kI2cAddressFailed);
    EXPECT_TRUE(sys.i2cWrites.empty());
    EXPECT_TRUE(called(sys, "close 7"));
    expectBusReleased(sys);
    EXPECT_EQ(bridgeWrites(regs), 0U);
}

// SR-HAL-10: a missing ACK is a codec configuration error, not a hang; nothing after it is sent.
TEST(CodecConfig, SR_HAL_10_NoAckStopsAndReportsTheRegister) {
    FakeCodecSys sys;
    sys.nackOnWrite = 2;  // the third write: R4
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kCodecNoAck);
    EXPECT_EQ(r.reg, 4);
    EXPECT_EQ(r.sysError, EREMOTEIO);
    EXPECT_EQ(sys.i2cWrites.size(), 3U);
    EXPECT_TRUE(called(sys, "close 7"));
    expectBusReleased(sys);
    EXPECT_EQ(bridgeWrites(regs), 0U);  // SR-AUD-09
    EXPECT_NE(describeCodecFailure(r, kTestCodecPaths).find("R4"), std::string::npos);
}

// The first write is the reset: a codec missing from the bus fails there (as on the board with
// HPS_I2C_CONTROL low, 19 Nov 2026).
TEST(CodecConfig, SR_HAL_10_NoCodecFailsOnReset) {
    FakeCodecSys sys;
    sys.nackOnWrite = 0;
    MockRegisterSpace regs;
    const CodecResult r = configureCodec(sys, regs, kTestCodecPaths);
    EXPECT_EQ(r.status, CodecStatus::kCodecNoAck);
    EXPECT_EQ(r.reg, 15);
    EXPECT_EQ(sys.i2cWrites.size(), 1U);
}

TEST(CodecConfig, SR_HAL_10_EveryFailureHasItsOwnMessage) {
    const std::array<CodecStatus, 5> failures{
        CodecStatus::kGpioChipNotFound, CodecStatus::kGpioSetupFailed, CodecStatus::kI2cOpenFailed,
        CodecStatus::kI2cAddressFailed, CodecStatus::kCodecNoAck};
    std::vector<std::string> messages;
    for (const CodecStatus s : failures) {
        CodecResult r;
        r.status = s;
        const std::string m = describeCodecFailure(r, kTestCodecPaths);
        EXPECT_EQ(m.rfind("codec configuration error", 0), 0U) << m;
        messages.push_back(m);
    }
    std::sort(messages.begin(), messages.end());
    EXPECT_EQ(std::adjacent_find(messages.begin(), messages.end()), messages.end());
    EXPECT_TRUE(describeCodecFailure(CodecResult{}, kTestCodecPaths).empty());
}
