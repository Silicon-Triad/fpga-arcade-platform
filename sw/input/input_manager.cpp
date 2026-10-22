// Input Manager: keyboard selection, non-blocking reads and key state (SR-INP-01 to SR-INP-08).
#include "input/input_manager.hpp"

#include <fcntl.h>

#include <algorithm>
#include <cstring>
#include <utility>
#include <vector>

namespace arcade {
namespace {

/// Most digits accepted after "event". Not an SRS value: a guard so the number fits in a long.
constexpr std::size_t kMaxEventDigits = 9;

/// Event number of a `/dev/input` entry named "event<N>", or -1 for any other name (SR-INP-01).
long eventNumber(const std::string& name) {
    const std::size_t prefixLen = std::strlen(InputManager::kEventPrefix);
    if (name.size() <= prefixLen || name.compare(0, prefixLen, InputManager::kEventPrefix) != 0) {
        return -1;
    }
    // Real event numbers are small; longer names are rejected so `number` cannot overflow.
    if (name.size() - prefixLen > kMaxEventDigits) {
        return -1;
    }
    long number = 0;
    for (std::size_t i = prefixLen; i < name.size(); ++i) {
        const char c = name[i];
        if (c < '0' || c > '9') {
            return -1;
        }
        number = number * 10 + (c - '0');
    }
    return number;
}

bool hasKey(const KeyBits& bits, std::uint16_t code) {
    return (bits[code / 8U] & (1U << (code % 8U))) != 0;
}

}  // namespace

InputManager::InputManager(EvdevSys& sys) : sys_(sys) {}

InputManager::~InputManager() {
    if (fd_ >= 0) {
        sys_.close(fd_);  // closing the device also releases the EVIOCGRAB grab
    }
}

bool InputManager::start() {
    // A repeated scan replaces the current keyboard; close it first so its fd and grab are freed.
    if (fd_ >= 0) {
        sys_.close(fd_);
        fd_ = -1;
        path_.clear();
    }
    // SR-INP-01: "first" device = lowest event number, so event2 comes before event10.
    std::vector<std::pair<long, std::string>> candidates;
    for (const std::string& name : sys_.listDir(kInputDir)) {
        const long number = eventNumber(name);
        if (number >= 0) {
            candidates.emplace_back(number, name);
        }
    }
    std::sort(candidates.begin(), candidates.end());

    for (const auto& candidate : candidates) {
        const std::string path = std::string(kInputDir) + "/" + candidate.second;
        // SR-INP-02: non-blocking, so pollInput() never waits for a key.
        const int fd = sys_.open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) {
            continue;
        }
        // SR-INP-01: skip mice and other devices without the game keys.
        // SR-INP-03: exclusive access; a device another process holds is not used.
        if (!isKeyboard(fd) || !sys_.grab(fd)) {
            sys_.close(fd);
            continue;
        }
        fd_ = fd;
        path_ = path;
        return true;
    }
    return false;
}

bool InputManager::isKeyboard(int fd) {
    KeyBits bits{};
    if (!sys_.keyBits(fd, bits)) {
        return false;
    }
    return std::all_of(kRequiredKeys.begin(), kRequiredKeys.end(),
                       [&bits](std::uint16_t code) { return hasKey(bits, code); });
}

std::size_t InputManager::readBatch() {
    int err = 0;
    const ssize_t bytes = sys_.read(fd_, buf_.data(), sizeof(buf_), err);
    if (bytes <= 0) {
        // SR-INP-04: EAGAIN means the queue is empty for this tick. Other errors also stop the
        // tick and leave `err` unused for now.
        // TODO(#22): on ENODEV enter the Disconnected state (SR-INP-12 to SR-INP-14).
        return 0;
    }
    // evdev returns whole events only.
    return static_cast<std::size_t>(bytes) / sizeof(input_event);
}

bool InputManager::translate(const input_event& raw, KeyEvent& out) {
    if (raw.type != EV_KEY || raw.code >= KEY_CNT) {
        return false;  // only EV_KEY is used (App. A, input_event)
    }
    switch (raw.value) {
        case kValuePress:  // SR-INP-05
            out = KeyEvent{KeyEventType::KeyDown, raw.code};
            pressed_[raw.code] = true;  // SR-INP-08
            return true;
        case kValueRelease:  // SR-INP-06
            out = KeyEvent{KeyEventType::KeyUp, raw.code};
            pressed_[raw.code] = false;  // SR-INP-08
            return true;
        case kValueRepeat:  // SR-INP-07: auto-repeat is discarded
        default:
            return false;
    }
}

}  // namespace arcade
