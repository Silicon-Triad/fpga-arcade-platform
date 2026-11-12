#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "hal/bridge_sys.hpp"
#include "hal/register_map.hpp"
#include "hal/register_space.hpp"

namespace arcade::hal {

/// SR-HAL-01, AS-01: the device the bridge window is mapped from.
inline constexpr std::string_view kDevMemPath = "/dev/mem";

/// Where the SR-HAL-09 readiness check reads the Linux state, and the value that means
/// "ready". Injected so tests use fake files. The board values are below; they were measured
/// on the board, they are not SRS values.
struct BridgeReadiness {
    std::string_view fpgaStatePath;        ///< SR-HAL-09: FPGA-manager configuration state file.
    std::string_view fpgaStateConfigured;  ///< Its contents when the FPGA is configured.
    std::string_view bridgeEnablePath;     ///< AS-02: the lightweight bridge's enable file.
    std::string_view bridgeEnabledValue;   ///< Its contents when the bridge is enabled.
};

/// SR-HAL-09, glossary "FPGA manager": the FPGA-manager configuration state file. This kernel
/// (3.18) uses Altera's FPGA manager driver, so the mainline /sys/class/fpga_manager/ does not
/// exist. Measured on the DE1-SoC, 12 Nov 2026, issue #34; not an SRS value.
inline constexpr std::string_view kFpgaManagerStatePath = "/sys/class/fpga/fpga0/status";
/// SR-HAL-09, glossary "FPGA manager": its contents (trailing white space removed) when the
/// FPGA is configured. It reads the same for U-Boot's default soc_system.rbf and for our design,
/// so only the ID/Version check tells them apart. Measured on the DE1-SoC, 12 Nov 2026, issue
/// #34. Any other contents mean "not configured"; no unconfigured value is assumed.
inline constexpr std::string_view kFpgaManagerConfiguredValue = "user mode";

/// SR-HAL-09, AS-02, glossary "FPGA manager": the lightweight bridge's enable file. Measured on
/// the DE1-SoC, 12 Nov 2026, issue #34; not an SRS value.
inline constexpr std::string_view kLwBridgeEnablePath = "/sys/class/fpga-bridge/lwhps2fpga/enable";
/// SR-HAL-09, AS-02, glossary "FPGA manager": its contents (trimmed) when the bridge is enabled.
/// Measured on the DE1-SoC, 12 Nov 2026, issue #34.
inline constexpr std::string_view kLwBridgeEnabledValue = "1";

/// The readiness files of the board image (SR-HAL-09, AS-02).
inline constexpr BridgeReadiness kBoardReadiness{kFpgaManagerStatePath, kFpgaManagerConfiguredValue,
                                                 kLwBridgeEnablePath, kLwBridgeEnabledValue};

/// SR-HAL-02: one distinct code per failure of open(). No call hangs, crashes or throws.
enum class BridgeStatus : std::uint8_t {
    kOk,                     ///< Mapped, and every readiness check passed.
    kFpgaStateUnreadable,    ///< The FPGA-manager state file could not be read (SR-HAL-09).
    kFpgaNotConfigured,      ///< The FPGA manager does not report "configured" (SR-HAL-09).
    kBridgeStateUnreadable,  ///< The lightweight bridge's enable file could not be read (AS-02).
    kBridgeNotEnabled,       ///< The lightweight bridge is not enabled (AS-02).
    kDevMemOpenFailed,       ///< open("/dev/mem") failed; `sysError` holds errno (AS-01).
    kMmapFailed,             ///< mmap() of the window failed; `sysError` holds errno (SR-HAL-01).
    kIdMismatch,             ///< ID/Version is not HAL_ID_MAGIC: not our design (SR-HAL-09).
    kFpgaNotReady,           ///< Status bit 1 (FPGA ready) is clear (§3.1.4; design choice).
};

/// The outcome of DevMemRegisterSpace::open() (SR-HAL-02).
struct BridgeResult {
    BridgeStatus status = BridgeStatus::kOk;
    int sysError = 0;         ///< errno of the failed call (unreadable file, open, mmap), else 0.
    std::uint32_t value = 0;  ///< The register read (ID/Version or Status) when it was wrong.
    std::string text;         ///< The state file's contents when they were wrong.
    bool ok() const noexcept { return status == BridgeStatus::kOk; }
};

/// The bridge window mapped from /dev/mem (SR-HAL-01, AS-01), behind the RegisterSpace seam.
/// Loads and stores are volatile, at the kOffset* byte offsets of register_map.hpp. Until
/// open() succeeds, and after release(), reads return 0 and writes are dropped: no access
/// is ever made through an unmapped pointer (SR-HAL-07, SR-HAL-09).
class DevMemRegisterSpace final : public RegisterSpace {
  public:
    /// Bind to the system calls and readiness files; both must outlive this object. No access.
    DevMemRegisterSpace(BridgeSys& sys, const BridgeReadiness& readiness) noexcept;
    /// Releases the window if it is still mapped (SR-HAL-07).
    ~DevMemRegisterSpace() override;
    DevMemRegisterSpace(const DevMemRegisterSpace&) = delete;
    DevMemRegisterSpace& operator=(const DevMemRegisterSpace&) = delete;
    DevMemRegisterSpace(DevMemRegisterSpace&&) = delete;
    DevMemRegisterSpace& operator=(DevMemRegisterSpace&&) = delete;

    /// SR-HAL-09, then SR-HAL-01, in this order, stopping at the first failure:
    /// (1) the FPGA-manager state file reads "configured"; (2) the lightweight bridge's enable
    /// file reads "enabled" (AS-02); neither step opens /dev/mem or touches the bridge;
    /// (3) open /dev/mem and mmap BRIDGE_WINDOW_SIZE bytes at BRIDGE_BASE; (4) checkFabric().
    /// On a failure after the mapping, the window is released again. Returns the first failure
    /// with its distinct code (SR-HAL-02), or kOk. If already mapped, returns kOk at once.
    ///
    /// noexcept, yet it builds std::string objects (sysfs text, paths): if memory runs out, the
    /// program terminates instead of returning an error. Accepted because this runs once at
    /// start-up, before any game tick (review of #115).
    BridgeResult open() noexcept;

    /// The readiness files this space checks (for describeBridgeFailure()).
    const BridgeReadiness& readiness() const noexcept { return readiness_; }

    /// True while the window is mapped (after a successful open(), before release()).
    bool isMapped() const noexcept { return base_ != nullptr; }

    std::uint8_t read8(std::uint32_t offset) noexcept override;
    void write8(std::uint32_t offset, std::uint8_t value) noexcept override;
    std::uint32_t read32(std::uint32_t offset) noexcept override;
    void write32(std::uint32_t offset, std::uint32_t value) noexcept override;

    /// SR-HAL-07: munmap the window and close /dev/mem, once; a second call does nothing.
    /// Writes no register, so Display Enable stays set. Afterwards reads return 0 and writes
    /// are dropped.
    void release() noexcept override;

  private:
    BridgeResult checkStateFile(std::string_view path, std::string_view expected,
                                BridgeStatus unreadable, BridgeStatus wrong) noexcept;
    bool inWindow(std::uint32_t offset, std::uint32_t width) const noexcept;

    BridgeSys& sys_;
    BridgeReadiness readiness_;
    volatile std::uint8_t* base_ = nullptr;
    int fd_ = -1;
};

/// SR-HAL-09 last step, on a mapped window: read ID/Version, and only if it equals HAL_ID_MAGIC
/// read Status and require bit 1 (FPGA ready, §3.1.4; our design choice, not named by
/// SR-HAL-09). Reads nothing else; never touches the tilemap, pattern or palette regions.
/// On a failure calls regs.release() and returns kIdMismatch or kFpgaNotReady with the value
/// read.
BridgeResult checkFabric(RegisterSpace& regs) noexcept;

/// SF-02: the failed step of `result` as one line for the console, naming the check, the file
/// or register, what was read and what was expected. Empty for kOk.
std::string describeBridgeFailure(const BridgeResult& result, const BridgeReadiness& readiness);

}  // namespace arcade::hal
