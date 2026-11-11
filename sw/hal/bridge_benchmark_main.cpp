// bridge_benchmark (#38): measures user-space write throughput over the lightweight
// HPS-to-FPGA bridge by writing one full tilemap bank again and again, and gives a verdict
// against the §3.1.4 worst-case data rate (12,288 bytes x 60 = 737,280 bytes/s). PR-04.
//
// Board only, as root. Run it only after arcade_top is loaded, the bridge is enabled and the
// ID register reads 0xA2CADE01 (bring-up guide steps d and f1): an access to the bridge
// window while the FPGA is unconfigured or the bridge is disabled can freeze the board.
//
// Safety: it reads ID / Version and Status first and stops without writing if either is
// wrong (the SR-HAL-09 rule). It writes only the back bank (never the front bank on screen)
// and never touches Control, Swap Request, patterns, palette or audio.
//
// Exit status: 0 every width met the target, 1 bad arguments, 2 /dev/mem could not be
// mapped, 3 hardware not ready, 4 a width was measured below the target.

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "hal/bridge_benchmark.hpp"
#include "hal/register_map.hpp"

namespace {

using namespace arcade::hal;
using namespace arcade::hal::bench;

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitMapFailed = 2;
constexpr int kExitNotReady = 3;
constexpr int kExitBelowTarget = 4;

/// The /dev/mem mapping of the bridge window (SR-HAL-01), unmapped on scope exit.
class BridgeWindow {
  public:
    BridgeWindow() {
        const int fd = ::open("/dev/mem", O_RDWR | O_SYNC);  // SR-HAL-01, AS-01
        if (fd < 0) {
            error_ = std::string("open /dev/mem: ") + std::strerror(errno);
            return;
        }
        void* p = ::mmap(nullptr, BRIDGE_WINDOW_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                         static_cast<off_t>(BRIDGE_BASE));  // SR-HAL-01: 2 MB at 0xFF20_0000
        const int mmapErrno = errno;
        static_cast<void>(::close(fd));  // the mapping stays valid after close
        if (p == MAP_FAILED) {
            error_ = std::string("mmap bridge window: ") + std::strerror(mmapErrno);
            return;
        }
        base_ = static_cast<volatile std::uint8_t*>(p);
    }
    ~BridgeWindow() {
        if (base_ != nullptr) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): munmap takes void*.
            static_cast<void>(::munmap(const_cast<std::uint8_t*>(base_), BRIDGE_WINDOW_SIZE));
        }
    }
    BridgeWindow(const BridgeWindow&) = delete;
    BridgeWindow& operator=(const BridgeWindow&) = delete;
    BridgeWindow(BridgeWindow&&) = delete;
    BridgeWindow& operator=(BridgeWindow&&) = delete;

    bool ok() const { return base_ != nullptr; }
    const std::string& error() const { return error_; }

    std::uint32_t read32(std::uint32_t offset) const {
        return *reinterpret_cast<volatile std::uint32_t*>(base_ + offset);
    }
    volatile std::uint8_t* bytes(std::uint32_t offset) const { return base_ + offset; }
    volatile std::uint32_t* words(std::uint32_t offset) const {
        return reinterpret_cast<volatile std::uint32_t*>(base_ + offset);
    }

  private:
    volatile std::uint8_t* base_ = nullptr;
    std::string error_;
};

std::string hex32(std::uint32_t v) {
    std::ostringstream s;
    s << "0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << v;
    return s.str();
}

/// Writes the back bank `passes` times and returns the elapsed nanoseconds. After each pass
/// a barrier and a read of ID / Version make sure the posted writes have reached the FPGA
/// before the next pass, so the time is for completed writes, not just CPU stores.
std::uint64_t timePasses(const BridgeWindow& w, std::uint32_t bankOffset, WriteWidth width,
                         const Options& opt) {
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t p = 0; p < opt.passes; ++p) {
        if (width == WriteWidth::k8) {
            writeBank8(w.bytes(bankOffset), opt.value);
        } else {
            writeBank32(w.words(bankOffset), opt.value);
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        static_cast<void>(w.read32(kOffsetIdVersion));
    }
    const auto end = std::chrono::steady_clock::now();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string error;
    const auto opt = parseOptions(args, error);
    if (!opt) {
        std::cerr << "bridge_benchmark: " << error << "\n" << usage();
        return kExitUsage;
    }
    if (opt->help) {
        std::cout << usage();
        return kExitOk;
    }

#if !defined(__arm__)
    // 0xFF20_0000 is the DE1-SoC HPS bridge only on the board; on a PC it is something else.
    std::cerr << "bridge_benchmark: board only (built for a non-ARM host). Nothing accessed.\n";
    return kExitNotReady;
#endif
    BridgeWindow w;
    if (!w.ok()) {
        std::cerr << "bridge_benchmark: " << w.error() << " (run as root)\n";
        return kExitMapFailed;
    }

    // SR-HAL-09 rule: confirm ID / Version and FPGA ready before any tilemap access.
    const std::uint32_t id = w.read32(kOffsetIdVersion);
    if (id != HAL_ID_MAGIC) {
        std::cerr << "graphics hardware not ready: ID / Version reads " << hex32(id)
                  << ", expected " << hex32(HAL_ID_MAGIC) << ". Nothing written.\n";
        return kExitNotReady;
    }
    const std::uint32_t status = w.read32(kOffsetStatusReg);
    if (((status >> kBitStatusFpgaReady) & 1u) == 0) {
        std::cerr << "graphics hardware not ready: Status " << hex32(status)
                  << " has FPGA ready (bit 1) clear. Nothing written.\n";
        return kExitNotReady;
    }

    const std::uint32_t front = w.read32(kOffsetSwapReqFrontBank);
    const std::uint32_t bank = backBankOffset(front);
    std::cout << "bridge_benchmark (#38, SRS 3.1.4 data rate, PR-04)\n"
              << "ID " << hex32(id) << ", Status " << hex32(status) << ", Front Bank "
              << (front & 1u) << "; writing back bank at offset " << hex32(bank) << "\n"
              << "bank size " << TILEMAP_BANK_SIZE << " B, passes " << opt->passes
              << ", tile value " << static_cast<unsigned>(opt->value) << ", target "
              << kTargetBytesPerSecond << " B/s\n";

    bool allPass = true;
    for (const WriteWidth width : opt->widths) {
        const std::uint64_t ns = timePasses(w, bank, width, *opt);
        const std::uint64_t bytes = static_cast<std::uint64_t>(TILEMAP_BANK_SIZE) * opt->passes;
        const std::uint64_t bps = bytesPerSecond(bytes, ns);
        const bool pass = meetsDataRate(bps);
        allPass = allPass && pass;
        const double msPerBank = static_cast<double>(ns) / 1e6 / opt->passes;
        std::cout << (width == WriteWidth::k8 ? " 8-bit" : "32-bit") << " stores: " << bytes
                  << " B in " << std::fixed << std::setprecision(3) << static_cast<double>(ns) / 1e9
                  << " s = " << bps << " B/s, " << msPerBank << " ms per full bank -> "
                  << (pass ? "PASS" : "FAIL") << "\n";
    }
    return allPass ? kExitOk : kExitBelowTarget;
}
