// Real Linux calls behind BridgeSys (SR-HAL-01, SR-HAL-09, AS-01).
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <limits>

#include "hal/bridge_sys.hpp"
#include "hal/register_map.hpp"

namespace arcade::hal {
namespace {

/// Bytes read from one sysfs attribute. Not an SRS value: the attributes read here hold one
/// short word ("1", a state name), so 64 bytes is ample; longer text is cut off and then fails
/// the comparison, which is the safe side.
constexpr std::size_t kSysfsReadBytes = 64;

// SR-HAL-01: the bridge base must be representable as a positive off_t (64-bit off_t on the
// board toolchain, _FILE_OFFSET_BITS=64), or mmap() would get a wrong offset.
static_assert(BRIDGE_BASE <= static_cast<std::uintmax_t>(std::numeric_limits<off_t>::max()),
              "BRIDGE_BASE does not fit in off_t");

}  // namespace

int LinuxBridgeSys::readTextFile(const std::string& path, std::string& text) noexcept {
    const int fd = ::open(path.c_str(), O_RDONLY);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (fd < 0) {
        return errno;
    }
    std::array<char, kSysfsReadBytes> buf{};
    const ssize_t n = ::read(fd, buf.data(), buf.size());
    const int readErr = (n < 0) ? errno : 0;
    static_cast<void>(::close(fd));
    if (n < 0) {
        return readErr;
    }
    text.assign(buf.data(), static_cast<std::size_t>(n));
    return 0;
}

int LinuxBridgeSys::open(const std::string& path, int flags, int& err) noexcept {
    const int fd = ::open(path.c_str(), flags);  // NOLINT(cppcoreguidelines-pro-type-vararg)
    err = (fd < 0) ? errno : 0;
    return fd;
}

void LinuxBridgeSys::close(int fd) noexcept { static_cast<void>(::close(fd)); }

void* LinuxBridgeSys::mmap(std::size_t length, int prot, int flags, int fd, std::uint64_t offset,
                           int& err) noexcept {
    void* p = ::mmap(nullptr, length, prot, flags, fd, static_cast<off_t>(offset));
    if (p == MAP_FAILED) {  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast): MAP_FAILED macro
        err = errno;
        return nullptr;
    }
    err = 0;
    return p;
}

void LinuxBridgeSys::munmap(void* addr, std::size_t length) noexcept {
    static_cast<void>(::munmap(addr, length));
}

}  // namespace arcade::hal
