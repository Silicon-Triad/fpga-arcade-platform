// Virtual keyboard through Linux uinput (PR-06 test tool).
#include "tools/uinput/uinput_device.hpp"

#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>

namespace arcade::uinput {

UinputDevice::~UinputDevice() {
    if (fd_ >= 0) {
        if (created_) {
            static_cast<void>(::ioctl(fd_, UI_DEV_DESTROY));  // NOLINT
        }
        static_cast<void>(::close(fd_));
    }
}

bool UinputDevice::fail(const std::string& what) {
    error_ = what + ": " + std::strerror(errno);
    return false;
}

bool UinputDevice::create(const std::string& uinputPath, const std::string& name,
                          const std::vector<std::uint16_t>& keys) {
    fd_ = ::open(uinputPath.c_str(), O_WRONLY | O_NONBLOCK | O_CLOEXEC);  // NOLINT
    if (fd_ < 0) {
        return fail("open " + uinputPath);
    }
    if (::ioctl(fd_, UI_SET_EVBIT, EV_KEY) < 0) {  // NOLINT
        return fail("UI_SET_EVBIT EV_KEY");
    }
    for (const std::uint16_t key : keys) {
        if (::ioctl(fd_, UI_SET_KEYBIT, key) < 0) {  // NOLINT
            return fail("UI_SET_KEYBIT " + std::to_string(key));
        }
    }
    // A virtual device: bus BUS_VIRTUAL, vendor, product and version left at 0.
    uinput_setup setup{};
    setup.id.bustype = BUS_VIRTUAL;
    std::strncpy(setup.name, name.c_str(), UINPUT_MAX_NAME_SIZE - 1);
    if (::ioctl(fd_, UI_DEV_SETUP, &setup) < 0) {  // NOLINT
        // Kernels before 4.5 have no UI_DEV_SETUP; they take a uinput_user_dev written to the
        // device instead (linux/uinput.h).
        uinput_user_dev legacy{};
        legacy.id = setup.id;
        std::copy(std::begin(setup.name), std::end(setup.name), std::begin(legacy.name));
        if (::write(fd_, &legacy, sizeof(legacy)) != static_cast<ssize_t>(sizeof(legacy))) {
            return fail("UI_DEV_SETUP and uinput_user_dev");
        }
    }
    if (::ioctl(fd_, UI_DEV_CREATE) < 0) {  // NOLINT
        return fail("UI_DEV_CREATE");
    }
    created_ = true;
    return true;
}

bool UinputDevice::write(std::uint16_t type, std::uint16_t code, std::int32_t value) {
    input_event ev{};
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (::write(fd_, &ev, sizeof(ev)) != static_cast<ssize_t>(sizeof(ev))) {
        return fail("write event");
    }
    return true;
}

bool UinputDevice::sendKey(std::uint16_t code, std::int32_t value) {
    return write(EV_KEY, code, value) && write(EV_SYN, SYN_REPORT, 0);
}

}  // namespace arcade::uinput
