#include "hal/dev_mem_register_space.hpp"

#include <fcntl.h>
#include <sys/mman.h>

#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <utility>

namespace arcade::hal {

namespace {

/// sysfs attributes end in a newline; compare the contents without trailing white space.
std::string_view trimRight(std::string_view s) noexcept {
    while (!s.empty() &&
           (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

std::string quoted(std::string_view s) { return "\"" + std::string(trimRight(s)) + "\""; }

std::string hex32(std::uint32_t v) {
    std::array<char, 16> buf{};
    (void)std::snprintf(buf.data(), buf.size(), "0x%08" PRIX32, v);
    return buf.data();
}

std::string sysText(int err) { return std::strerror(err); }

}  // namespace

DevMemRegisterSpace::DevMemRegisterSpace(BridgeSys& sys, const BridgeReadiness& readiness) noexcept
    : sys_(sys), readiness_(readiness) {}

DevMemRegisterSpace::~DevMemRegisterSpace() { release(); }

BridgeResult DevMemRegisterSpace::checkStateFile(std::string_view path, std::string_view expected,
                                                 BridgeStatus unreadable,
                                                 BridgeStatus wrong) noexcept {
    BridgeResult r;
    std::string text;
    const int err = sys_.readTextFile(std::string(path), text);
    if (err != 0) {
        r.status = unreadable;
        r.sysError = err;
    } else if (trimRight(text) != expected) {
        r.status = wrong;
        r.text = std::move(text);
    }
    return r;
}

BridgeResult DevMemRegisterSpace::open() noexcept {
    if (isMapped()) {
        return {};
    }
    // SR-HAL-09 step 1: the FPGA manager says the fabric is configured. No bridge access yet:
    // an access while the fabric is unconfigured can hang the HPS (SR-HAL-09 rationale).
    BridgeResult r =
        checkStateFile(readiness_.fpgaStatePath, readiness_.fpgaStateConfigured,
                       BridgeStatus::kFpgaStateUnreadable, BridgeStatus::kFpgaNotConfigured);
    if (!r.ok()) {
        return r;
    }
    // Step 2 (design choice supporting AS-02): the lightweight bridge is enabled, since an
    // access through a disabled bridge can hang the HPS too. Still no bridge access.
    r = checkStateFile(readiness_.bridgeEnablePath, readiness_.bridgeEnabledValue,
                       BridgeStatus::kBridgeStateUnreadable, BridgeStatus::kBridgeNotEnabled);
    if (!r.ok()) {
        return r;
    }
    // SR-HAL-01, AS-01: map the 2 MB window at 0xFF20_0000 from /dev/mem (root only). O_SYNC
    // gives an uncached mapping, as in the board-tested bridge_benchmark (#38).
    int err = 0;
    const int fd = sys_.open(std::string(kDevMemPath), O_RDWR | O_SYNC, err);
    if (fd < 0) {
        r.status = BridgeStatus::kDevMemOpenFailed;  // SR-HAL-02
        r.sysError = err;
        return r;
    }
    void* p = sys_.mmap(BRIDGE_WINDOW_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                        static_cast<std::uint64_t>(BRIDGE_BASE), err);
    if (p == nullptr) {
        sys_.close(fd);
        r.status = BridgeStatus::kMmapFailed;  // SR-HAL-02
        r.sysError = err;
        return r;
    }
    fd_ = fd;
    base_ = static_cast<volatile std::uint8_t*>(p);
    // SR-HAL-09 last step: ID/Version (then FPGA ready); releases the window on a failure.
    return checkFabric(*this);
}

bool DevMemRegisterSpace::inWindow(std::uint32_t offset, std::uint32_t width) const noexcept {
    return base_ != nullptr && offset <= BRIDGE_WINDOW_SIZE - width && (offset % width) == 0;
}

std::uint8_t DevMemRegisterSpace::read8(std::uint32_t offset) noexcept {
    return inWindow(offset, 1) ? base_[offset] : 0;
}

void DevMemRegisterSpace::write8(std::uint32_t offset, std::uint8_t value) noexcept {
    if (inWindow(offset, 1)) {
        base_[offset] = value;
    }
}

std::uint32_t DevMemRegisterSpace::read32(std::uint32_t offset) noexcept {
    if (!inWindow(offset, 4)) {
        return 0;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): a device register.
    return *reinterpret_cast<volatile std::uint32_t*>(base_ + offset);
}

void DevMemRegisterSpace::write32(std::uint32_t offset, std::uint32_t value) noexcept {
    if (inWindow(offset, 4)) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): a device register.
        *reinterpret_cast<volatile std::uint32_t*>(base_ + offset) = value;
    }
}

void DevMemRegisterSpace::release() noexcept {
    if (base_ == nullptr) {
        return;  // never mapped, or already released: nothing to do
    }
    // SR-HAL-07: forget the pointer first, so no access can use it; no register write.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): munmap takes void*.
    void* p = const_cast<std::uint8_t*>(base_);
    base_ = nullptr;
    sys_.munmap(p, BRIDGE_WINDOW_SIZE);
    sys_.close(fd_);
    fd_ = -1;
}

BridgeResult checkFabric(RegisterSpace& regs) noexcept {
    BridgeResult r;
    // SR-HAL-09: the ID/Version register confirms our design is loaded. On this board a fresh
    // boot loads U-Boot's default soc_system.rbf, which is configured but is not our design.
    const std::uint32_t id = regs.read32(kOffsetIdVersion);
    if (id != HAL_ID_MAGIC) {
        r.status = BridgeStatus::kIdMismatch;
        r.value = id;
    } else {
        // Design choice (§3.1.4 Status bit 1): our design out of reset and ready. Not named by
        // SR-HAL-09; read only once the ID shows the register map is ours.
        const std::uint32_t status = regs.read32(kOffsetStatusReg);
        if (((status >> kBitStatusFpgaReady) & 1U) == 0) {
            r.status = BridgeStatus::kFpgaNotReady;
            r.value = status;
        }
    }
    if (!r.ok()) {
        regs.release();  // SR-HAL-09: nothing more is accessed
    }
    return r;
}

std::string describeBridgeFailure(const BridgeResult& result, const BridgeReadiness& readiness) {
    const std::string fpgaFile(readiness.fpgaStatePath);
    const std::string bridgeFile(readiness.bridgeEnablePath);
    switch (result.status) {
        case BridgeStatus::kOk:
            return {};
        case BridgeStatus::kFpgaStateUnreadable:
            return "check FPGA manager state: cannot read " + fpgaFile + " (" +
                   sysText(result.sysError) + ")";
        case BridgeStatus::kFpgaNotConfigured:
            return "check FPGA manager state: " + fpgaFile + " reads " + quoted(result.text) +
                   ", expected " + quoted(readiness.fpgaStateConfigured) + " (FPGA not configured)";
        case BridgeStatus::kBridgeStateUnreadable:
            return "check lightweight bridge enabled: cannot read " + bridgeFile + " (" +
                   sysText(result.sysError) + ")";
        case BridgeStatus::kBridgeNotEnabled:
            return "check lightweight bridge enabled: " + bridgeFile + " reads " +
                   quoted(result.text) + ", expected " + quoted(readiness.bridgeEnabledValue);
        case BridgeStatus::kDevMemOpenFailed:
            return "open " + std::string(kDevMemPath) + ": " + sysText(result.sysError) +
                   " (run as root, AS-01)";
        case BridgeStatus::kMmapFailed:
            return "map the bridge window (" + hex32(static_cast<std::uint32_t>(BRIDGE_BASE)) +
                   ", " + std::to_string(BRIDGE_WINDOW_SIZE) +
                   " bytes): " + sysText(result.sysError);
        case BridgeStatus::kIdMismatch:
            return "check ID/Version register: reads " + hex32(result.value) + ", expected " +
                   hex32(HAL_ID_MAGIC) + " (our FPGA design is not loaded)";
        case BridgeStatus::kFpgaNotReady:
            return "check Status FPGA ready: Status reads " + hex32(result.value) +
                   ", FPGA ready (bit 1) is clear";
    }
    return "map the lightweight bridge";  // unreachable: every status is handled above
}

}  // namespace arcade::hal
