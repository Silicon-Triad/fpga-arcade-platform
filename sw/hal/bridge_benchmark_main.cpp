// bridge_benchmark: LW-bridge tilemap write throughput on the DE1-SoC (issue #38,
// §3.1.4 open item "Data rate", PR-04). Run as root on the board with the arcade bitstream
// loaded:  ./bridge_benchmark [passes]
//
// Benchmark-only /dev/mem mapping: the HAL's own mapping (SR-HAL-01, 02) is issue #34. This
// file uses the same window (BRIDGE_BASE, BRIDGE_WINDOW_SIZE from register_map.hpp) and
// volatile stores, so it measures the bridge, not a cached copy.

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "hal/bridge_benchmark.hpp"
#include "hal/register_map.hpp"

namespace {

using arcade::hal::BRIDGE_BASE;
using arcade::hal::BRIDGE_WINDOW_SIZE;

/// Volatile view of the mapped LW-bridge window (§3.1.4). Benchmark use only.
class DevMemWindow final : public arcade::hal::RegisterSpace {
  public:
    explicit DevMemWindow(volatile std::uint8_t* base) noexcept : base_(base) {}
    std::uint8_t read8(std::uint32_t offset) noexcept override { return base_[offset]; }
    void write8(std::uint32_t offset, std::uint8_t value) noexcept override {
        base_[offset] = value;
    }
    std::uint32_t read32(std::uint32_t offset) noexcept override {
        return *reinterpret_cast<volatile std::uint32_t*>(base_ + offset);
    }
    void write32(std::uint32_t offset, std::uint32_t value) noexcept override {
        *reinterpret_cast<volatile std::uint32_t*>(base_ + offset) = value;
    }

  private:
    volatile std::uint8_t* base_;
};

constexpr std::uint32_t kDefaultPasses = 600;  // Benchmark choice, not an SRS value.

}  // namespace

int main(int argc, char** argv) {
    std::uint32_t passes = kDefaultPasses;
    if (argc > 1) {
        char* end = nullptr;
        const unsigned long v = std::strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || v == 0 || v > 1000000UL) {
            (void)std::fprintf(stderr, "usage: %s [passes 1..1000000]\n", argv[0]);
            return 2;
        }
        passes = static_cast<std::uint32_t>(v);
    }

    const int fd = ::open("/dev/mem", O_RDWR | O_SYNC);  // §3.1.4: mmap() on /dev/mem
    if (fd < 0) {
        (void)std::fprintf(stderr, "open /dev/mem: %s (run as root)\n", std::strerror(errno));
        return 1;
    }
    void* map = ::mmap(nullptr, BRIDGE_WINDOW_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                       static_cast<off_t>(BRIDGE_BASE));
    ::close(fd);
    if (map == MAP_FAILED) {
        (void)std::fprintf(stderr, "mmap bridge window: %s\n", std::strerror(errno));
        return 1;
    }

    DevMemWindow window(static_cast<volatile std::uint8_t*>(map));
    arcade::hal::SteadyClock clock;
    const auto r = arcade::hal::benchmarkTilemapWrites(window, clock, passes);
    ::munmap(map, BRIDGE_WINDOW_SIZE);

    std::printf("passes            %u\n", passes);
    std::printf("back bank offset  0x%04X\n", static_cast<unsigned>(r.backBankOffset));
    std::printf("bytes written     %llu\n", static_cast<unsigned long long>(r.bytesWritten));
    std::printf("elapsed ns        %lld\n", static_cast<long long>(r.elapsed.count()));
    std::printf("bytes/s           %llu\n", static_cast<unsigned long long>(r.bytesPerSecond));
    std::printf("target bytes/s    %llu (12288 x 60, SRS 3.1.4)\n",
                static_cast<unsigned long long>(arcade::hal::kWorstCaseTilemapBytesPerSecond));
    std::printf("verdict           %s\n", r.meetsWorstCase ? "PASS" : "FAIL");
    return r.meetsWorstCase ? 0 : 3;
}
