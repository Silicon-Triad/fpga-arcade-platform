// Unit tests for the /dev/mem bridge window and readiness check (issue #34; SR-HAL-01,
// SR-HAL-02, SR-HAL-07, SR-HAL-09, AS-01, AS-02), on fake system calls.
#include "hal/dev_mem_register_space.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/mman.h>

#include <array>
#include <cerrno>
#include <set>
#include <string>
#include <vector>

#include "fake_bridge_sys.hpp"
#include "hal/hal_lifecycle.hpp"
#include "hal/tilemap_hal.hpp"
#include "mock_register_space.hpp"

namespace {
using namespace arcade::hal;
using arcade::hal::test::Access;
using arcade::hal::test::FakeBridgeSys;
using arcade::hal::test::FakeClock;
using arcade::hal::test::kTestReadiness;
using arcade::hal::test::MockRegisterSpace;

const std::string kFpgaFile(kTestReadiness.fpgaStatePath);
const std::string kBridgeFile(kTestReadiness.bridgeEnablePath);

struct DevMemFixture : ::testing::Test {
    FakeBridgeSys sys;
    DevMemRegisterSpace space{sys, kTestReadiness};

    /// True if neither /dev/mem was opened nor anything mapped (no bridge access possible).
    bool noDevMem() const { return sys.count("open") == 0 && sys.count("mmap") == 0; }
    bool windowIsAllZero() const {
        for (const std::uint32_t w : sys.window) {
            if (w != 0) {
                return false;
            }
        }
        return true;
    }
};

// --- SR-HAL-09 step 1: FPGA-manager state -----------------------------------------------------

TEST_F(DevMemFixture, SR_HAL_02_FpgaStateUnreadableHasItsOwnCodeAndNoDevMemAccess) {
    sys.files.erase(kFpgaFile);
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kFpgaStateUnreadable);
    EXPECT_EQ(r.sysError, ENOENT);
    EXPECT_TRUE(noDevMem());
    EXPECT_EQ(sys.count("read " + kBridgeFile), 0);  // stops at the first failed step
    EXPECT_FALSE(space.isMapped());
}

TEST_F(DevMemFixture, SR_HAL_09_FpgaNotConfiguredStopsBeforeAnyBridgeAccess) {
    sys.files[kFpgaFile] = {0, "unknown\n"};
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kFpgaNotConfigured);
    EXPECT_EQ(r.text, "unknown\n");
    EXPECT_TRUE(noDevMem());
    EXPECT_EQ(sys.count("read " + kBridgeFile), 0);
}

TEST_F(DevMemFixture, SR_HAL_09_FpgaStateMustMatchExactlyApartFromTrailingNewline) {
    sys.files[kFpgaFile] = {0, "configured but not"};
    EXPECT_EQ(space.open().status, BridgeStatus::kFpgaNotConfigured);
    sys.files[kFpgaFile] = {0, ""};
    EXPECT_EQ(space.open().status, BridgeStatus::kFpgaNotConfigured);
    EXPECT_TRUE(noDevMem());
}

// --- Step 2 (AS-02): lightweight bridge enabled ----------------------------------------------

TEST_F(DevMemFixture, SR_HAL_02_BridgeStateUnreadableHasItsOwnCodeAndNoDevMemAccess) {
    sys.files[kBridgeFile] = {EACCES, ""};
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kBridgeStateUnreadable);
    EXPECT_EQ(r.sysError, EACCES);
    EXPECT_TRUE(noDevMem());
}

TEST_F(DevMemFixture, AS_02_BridgeNotEnabledStopsBeforeAnyBridgeAccess) {
    sys.files[kBridgeFile] = {0, "0\n"};
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kBridgeNotEnabled);
    EXPECT_EQ(r.text, "0\n");
    EXPECT_TRUE(noDevMem());
    // The FPGA-manager state was checked first.
    ASSERT_GE(sys.calls.size(), 2U);
    EXPECT_EQ(sys.calls[0], "read " + kFpgaFile);
    EXPECT_EQ(sys.calls[1], "read " + kBridgeFile);
}

// --- SR-HAL-01, AS-01: /dev/mem and mmap -----------------------------------------------------

TEST_F(DevMemFixture, AS_01_DevMemOpenFailureKeepsErrno) {
    sys.openErr = EACCES;  // not root
    BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kDevMemOpenFailed);
    EXPECT_EQ(r.sysError, EACCES);
    EXPECT_EQ(sys.count("mmap"), 0);
    EXPECT_TRUE(sys.closedFds.empty());

    sys.openErr = ENOENT;  // no /dev/mem
    r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kDevMemOpenFailed);
    EXPECT_EQ(r.sysError, ENOENT);
}

TEST_F(DevMemFixture, SR_HAL_02_MmapFailureHasItsOwnCodeAndClosesDevMem) {
    sys.loadOurDesign();
    sys.mmapErr = EINVAL;
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kMmapFailed);
    EXPECT_EQ(r.sysError, EINVAL);
    EXPECT_EQ(sys.closedFds, std::vector<int>{FakeBridgeSys::kFd});
    EXPECT_EQ(sys.count("munmap"), 0);
    EXPECT_FALSE(space.isMapped());
    EXPECT_EQ(space.read32(kOffsetIdVersion), 0U);  // nothing mapped: no access
}

TEST_F(DevMemFixture, SR_HAL_01_SuccessMapsTheWindowOnceAtBridgeBase) {
    sys.loadOurDesign();
    const BridgeResult r = space.open();
    ASSERT_TRUE(r.ok());
    EXPECT_TRUE(space.isMapped());
    EXPECT_EQ(sys.openPath, "/dev/mem");
    EXPECT_EQ(sys.openFlags, O_RDWR | O_SYNC);
    EXPECT_EQ(sys.count("mmap"), 1);
    EXPECT_EQ(sys.mmapLength, BRIDGE_WINDOW_SIZE);                       // SR-HAL-01: 2 MB
    EXPECT_EQ(sys.mmapOffset, static_cast<std::uint64_t>(BRIDGE_BASE));  // 0xFF20_0000
    EXPECT_EQ(sys.mmapProt, PROT_READ | PROT_WRITE);
    EXPECT_EQ(sys.mmapFlags, MAP_SHARED);
    EXPECT_EQ(sys.mmapFd, FakeBridgeSys::kFd);
    // Steps in order: FPGA manager, bridge enable, open, mmap.
    const std::vector<std::string> expected{"read " + kFpgaFile, "read " + kBridgeFile,
                                            "open /dev/mem", "mmap"};
    EXPECT_EQ(sys.calls, expected);
    // A second open() does not map again.
    EXPECT_TRUE(space.open().ok());
    EXPECT_EQ(sys.count("mmap"), 1);
}

TEST_F(DevMemFixture, SR_HAL_01_AccessesReachTheWindowAtTheirByteOffsets) {
    sys.loadOurDesign();
    ASSERT_TRUE(space.open().ok());
    EXPECT_EQ(space.read32(kOffsetIdVersion), HAL_ID_MAGIC);
    space.write32(kOffsetControlReg, 0x4U);
    EXPECT_EQ(sys.peek32(kOffsetControlReg), 0x4U);
    space.write8(kOffsetTilemapBank1 + 5, 0xAB);
    EXPECT_EQ(sys.bytes()[kOffsetTilemapBank1 + 5], 0xAB);
    sys.bytes()[kOffsetTilePatternRam + 3] = 0x5C;
    EXPECT_EQ(space.read8(kOffsetTilePatternRam + 3), 0x5C);
    const std::array<std::uint8_t, 3> block{1, 2, 3};
    space.writeBlock8(kOffsetTilemapBank0 + 10, block.data(), block.size());
    EXPECT_EQ(sys.bytes()[kOffsetTilemapBank0 + 12], 3);
}

TEST_F(DevMemFixture, SR_HAL_02_AccessesOutsideTheWindowOrUnalignedAreDropped) {
    sys.loadOurDesign();
    ASSERT_TRUE(space.open().ok());
    const auto size = static_cast<std::uint32_t>(BRIDGE_WINDOW_SIZE);
    EXPECT_EQ(space.read8(size), 0U);
    EXPECT_EQ(space.read32(size - 2), 0U);
    EXPECT_EQ(space.read32(kOffsetIdVersion + 1), 0U);  // not 32-bit aligned
    space.write32(size, 1U);
    space.write8(size, 1U);
    space.write32(kOffsetControlReg + 2, 0xFFFFFFFFU);
    EXPECT_EQ(sys.peek32(kOffsetControlReg), 0U);
}

// --- SR-HAL-09 last step: ID/Version (and FPGA ready) ----------------------------------------

TEST_F(DevMemFixture, SR_HAL_09_IdMismatchReleasesTheWindowAndWritesNothing) {
    // Fresh boot: U-Boot's default soc_system.rbf is configured, but it is not our design.
    sys.poke32(kOffsetIdVersion, 0x12345678U);
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kIdMismatch);
    EXPECT_EQ(r.value, 0x12345678U);
    EXPECT_FALSE(space.isMapped());
    EXPECT_EQ(sys.count("munmap"), 1);
    EXPECT_EQ(sys.munmapLength, BRIDGE_WINDOW_SIZE);
    EXPECT_EQ(sys.munmapAddr, static_cast<void*>(sys.window.data()));
    EXPECT_EQ(sys.closedFds, std::vector<int>{FakeBridgeSys::kFd});
    // Nothing was written: only the ID value the test put there is non-zero.
    sys.poke32(kOffsetIdVersion, 0);
    EXPECT_TRUE(windowIsAllZero());
}

TEST(CheckFabric, SR_HAL_09_IdMismatchReadsOnlyTheIdRegisterThenReleases) {
    MockRegisterSpace regs;
    regs.idVersion = 0;  // not our design
    const BridgeResult r = checkFabric(regs);
    EXPECT_EQ(r.status, BridgeStatus::kIdMismatch);
    ASSERT_EQ(regs.log.size(), 1U);
    EXPECT_EQ(regs.log[0].kind, Access::Kind::kRead32);
    EXPECT_EQ(regs.log[0].offset, kOffsetIdVersion);
    EXPECT_EQ(regs.releaseCalls, 1);
    EXPECT_EQ(regs.accessesAfterRelease, 0);
}

TEST(CheckFabric, SR_HAL_09_FpgaReadyClearIsItsOwnFailureAfterTheId) {
    MockRegisterSpace regs;
    regs.idVersion = HAL_ID_MAGIC;
    regs.status = 1U << kBitStatusVsync;  // FPGA ready (bit 1) clear
    const BridgeResult r = checkFabric(regs);
    EXPECT_EQ(r.status, BridgeStatus::kFpgaNotReady);
    EXPECT_EQ(r.value, regs.status);
    ASSERT_EQ(regs.log.size(), 2U);
    EXPECT_EQ(regs.log[0].offset, kOffsetIdVersion);
    EXPECT_EQ(regs.log[1].offset, kOffsetStatusReg);
    EXPECT_EQ(regs.releaseCalls, 1);
}

TEST(CheckFabric, SR_HAL_09_OurDesignReadyPassesWithTwoReadsAndNoWrite) {
    MockRegisterSpace regs;
    regs.idVersion = HAL_ID_MAGIC;
    regs.status = (1U << kBitStatusFpgaReady) | (1U << kBitStatusVsync);  // 0x3, as on the board
    EXPECT_TRUE(checkFabric(regs).ok());
    ASSERT_EQ(regs.log.size(), 2U);
    for (const auto& a : regs.log) {
        EXPECT_EQ(a.kind, Access::Kind::kRead32);
    }
    EXPECT_EQ(regs.releaseCalls, 0);
}

TEST_F(DevMemFixture, SR_HAL_09_FpgaNotReadyReleasesTheWindow) {
    sys.poke32(kOffsetIdVersion, HAL_ID_MAGIC);  // Status stays 0
    const BridgeResult r = space.open();
    EXPECT_EQ(r.status, BridgeStatus::kFpgaNotReady);
    EXPECT_EQ(r.value, 0U);
    EXPECT_FALSE(space.isMapped());
    EXPECT_EQ(sys.count("munmap"), 1);
}

// --- SR-HAL-07: release() --------------------------------------------------------------------

TEST_F(DevMemFixture, SR_HAL_07_ReleaseUnmapsAndClosesOnceThenDropsEveryAccess) {
    sys.loadOurDesign();
    ASSERT_TRUE(space.open().ok());
    space.release();
    EXPECT_EQ(sys.count("munmap"), 1);
    EXPECT_EQ(sys.munmapLength, BRIDGE_WINDOW_SIZE);
    EXPECT_EQ(sys.closedFds, std::vector<int>{FakeBridgeSys::kFd});
    EXPECT_FALSE(space.isMapped());

    space.release();  // second release: harmless
    EXPECT_EQ(sys.count("munmap"), 1);
    EXPECT_EQ(sys.count("close"), 1);

    EXPECT_EQ(space.read32(kOffsetIdVersion), 0U);
    EXPECT_EQ(space.read8(kOffsetTilemapBank0), 0U);
    space.write32(kOffsetControlReg, 0x4U);
    space.write8(kOffsetTilemapBank0, 9);
    EXPECT_EQ(sys.peek32(kOffsetControlReg), 0U);
    EXPECT_EQ(sys.bytes()[kOffsetTilemapBank0], 0U);
}

TEST_F(DevMemFixture, SR_HAL_07_ReleaseWithoutAMappingDoesNothing) {
    space.release();
    EXPECT_TRUE(sys.calls.empty());
}

TEST(DevMemLifetime, SR_HAL_07_DestructorReleasesAStillMappedWindowOnce) {
    FakeBridgeSys sys;
    sys.loadOurDesign();
    {
        DevMemRegisterSpace space(sys, kTestReadiness);
        ASSERT_TRUE(space.open().ok());
    }
    EXPECT_EQ(sys.count("munmap"), 1);
    EXPECT_EQ(sys.count("close"), 1);
}

TEST_F(DevMemFixture, SR_HAL_07_PresentAfterShutdownTouchesNothing) {
    sys.loadOurDesign();
    ASSERT_TRUE(space.open().ok());
    FakeClock clock;
    TilemapHal tilemap(space, clock);
    HalLifecycle lifecycle(space);
    ASSERT_EQ(tilemap.setTile(1, 2, 7), HalStatus::kOk);
    lifecycle.shutdown();
    EXPECT_EQ(sys.count("munmap"), 1);
    const std::vector<std::uint32_t> before = sys.window;
    const std::vector<std::string> callsBefore = sys.calls;
    (void)tilemap.present();  // a late present(): every access is dropped
    (void)tilemap.frameCounter();
    EXPECT_EQ(sys.window, before);
    EXPECT_EQ(sys.calls, callsBefore);  // no further system call either
    lifecycle.shutdown();
    EXPECT_EQ(sys.count("munmap"), 1);
}

// --- SR-HAL-02 / SF-02: distinct codes and messages ------------------------------------------

TEST(DescribeBridgeFailure, SR_HAL_02_EveryFailureHasADistinctMessage) {
    const std::vector<BridgeStatus> failures{BridgeStatus::kFpgaStateUnreadable,
                                             BridgeStatus::kFpgaNotConfigured,
                                             BridgeStatus::kBridgeStateUnreadable,
                                             BridgeStatus::kBridgeNotEnabled,
                                             BridgeStatus::kDevMemOpenFailed,
                                             BridgeStatus::kMmapFailed,
                                             BridgeStatus::kIdMismatch,
                                             BridgeStatus::kFpgaNotReady};
    std::set<std::string> messages;
    for (const BridgeStatus s : failures) {
        BridgeResult r;
        r.status = s;
        r.sysError = EACCES;
        r.text = "0\n";
        const std::string m = describeBridgeFailure(r, kTestReadiness);
        EXPECT_FALSE(m.empty());
        messages.insert(m);
    }
    EXPECT_EQ(messages.size(), failures.size());
    EXPECT_TRUE(describeBridgeFailure(BridgeResult{}, kTestReadiness).empty());
}

TEST(DescribeBridgeFailure, SF_02_MessagesNameTheFileOrRegisterAndWhatWasRead) {
    BridgeResult r;
    r.status = BridgeStatus::kIdMismatch;
    r.value = 0x0000BEEFU;
    EXPECT_EQ(describeBridgeFailure(r, kTestReadiness),
              "check ID/Version register: reads 0x0000BEEF, expected 0xA2CADE01 (our FPGA "
              "design is not loaded)");
    r = {};
    r.status = BridgeStatus::kBridgeNotEnabled;
    r.text = "0\n";
    EXPECT_EQ(describeBridgeFailure(r, kTestReadiness),
              "check lightweight bridge enabled: /fake/bridge/enable reads \"0\", expected \"1\"");
    r = {};
    r.status = BridgeStatus::kDevMemOpenFailed;
    r.sysError = EACCES;
    EXPECT_EQ(describeBridgeFailure(r, kTestReadiness),
              "open /dev/mem: Permission denied (run as root, AS-01)");
    r.status = BridgeStatus::kFpgaStateUnreadable;
    r.sysError = ENOENT;
    EXPECT_EQ(describeBridgeFailure(r, kTestReadiness),
              "check FPGA manager state: cannot read /fake/fpga/state (No such file or directory)");
    r = {};
    r.status = BridgeStatus::kMmapFailed;
    r.sysError = EINVAL;
    EXPECT_EQ(describeBridgeFailure(r, kTestReadiness),
              "map the bridge window (0xFF200000, 2097152 bytes): Invalid argument");
}

}  // namespace
