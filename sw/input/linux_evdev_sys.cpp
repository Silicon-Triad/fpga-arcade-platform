// Real Linux calls behind EvdevSys (SR-INP-01 to SR-INP-04).
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <filesystem>
#include <system_error>

#include "input/evdev_sys.hpp"

namespace arcade {

std::vector<std::string> LinuxEvdevSys::listDir(const std::string& dir) {
    std::vector<std::string> names;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        names.push_back(it->path().filename().string());
    }
    return names;
}

int LinuxEvdevSys::open(const std::string& path, int flags) {
    return ::open(path.c_str(), flags);  // NOLINT(cppcoreguidelines-pro-type-vararg)
}

void LinuxEvdevSys::close(int fd) { static_cast<void>(::close(fd)); }

bool LinuxEvdevSys::keyBits(int fd, KeyBits& bits) {
    bits.fill(0);
    // SR-INP-01: ask the kernel which key codes this device can report.
    return ::ioctl(fd, EVIOCGBIT(EV_KEY, bits.size()), bits.data()) >= 0;  // NOLINT
}

bool LinuxEvdevSys::grab(int fd) {
    // SR-INP-03: exclusive access, so keys do not also reach the Linux console.
    return ::ioctl(fd, EVIOCGRAB, 1) == 0;  // NOLINT
}

ssize_t LinuxEvdevSys::read(int fd, void* buf, std::size_t len, int& err) {
    const ssize_t n = ::read(fd, buf, len);
    err = (n < 0) ? errno : 0;
    return n;
}

}  // namespace arcade
