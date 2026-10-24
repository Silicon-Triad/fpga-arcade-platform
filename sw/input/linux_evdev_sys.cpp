// Real Linux calls behind EvdevSys (SR-INP-01 to SR-INP-04, SR-INP-12, SR-INP-15).
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <filesystem>
#include <system_error>

#include "input/evdev_sys.hpp"

namespace arcade {
namespace {

/// Bytes read from the inotify fd per read() call. Not an SRS value: readDirChanges() repeats the
/// call until EAGAIN, so this only sets how many events one call can return.
constexpr std::size_t kInotifyBufBytes = 1024;
// inotify(7): read() fails with EINVAL if the buffer cannot hold one event with the longest name.
static_assert(kInotifyBufBytes >= sizeof(inotify_event) + NAME_MAX + 1, "inotify buffer");

}  // namespace

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

int LinuxEvdevSys::watchDir(const std::string& dir) {
    // SR-INP-15: inotify reports new device nodes without polling the directory. Non-blocking,
    // so readDirChanges() never waits (SR-INP-02).
    const int fd = ::inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd < 0) {
        return -1;
    }
    // IN_CREATE: a new eventN node appeared. IN_ATTRIB: its owner or mode changed. The second is
    // needed because the node can appear before it is readable by the `input` group (SC-01),
    // so the first scan after IN_CREATE may fail to open it (see sw/input/README.md).
    if (::inotify_add_watch(fd, dir.c_str(), IN_CREATE | IN_ATTRIB) < 0) {
        static_cast<void>(::close(fd));
        return -1;
    }
    return fd;
}

void LinuxEvdevSys::readDirChanges(int watchFd, DirChangeSink& sink) {
    // Large enough for several events with names; the loop drains whatever does not fit.
    std::array<char, kInotifyBufBytes> buf{};
    for (;;) {
        const ssize_t n = ::read(watchFd, buf.data(), buf.size());
        if (n <= 0) {
            return;  // EAGAIN: queue empty
        }
        const auto end = static_cast<std::size_t>(n);
        // inotify(7): each record is an inotify_event followed by `len` bytes holding the
        // NUL-padded entry name; the kernel only returns whole records.
        for (std::size_t off = 0; off + sizeof(inotify_event) <= end;) {
            inotify_event ev{};
            std::memcpy(&ev, buf.data() + off, sizeof(ev));  // the buffer may be unaligned
            if ((ev.mask & IN_Q_OVERFLOW) != 0U) {
                sink.eventsLost();
            } else if (ev.len > 0) {
                sink.entryChanged(buf.data() + off + sizeof(inotify_event));
            }
            off += sizeof(inotify_event) + ev.len;
        }
    }
}

}  // namespace arcade
