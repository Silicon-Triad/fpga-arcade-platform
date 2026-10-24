// Input Manager: keyboard selection, non-blocking reads, key state and hot-plug
// (SR-INP-01 to SR-INP-08, SR-INP-12 to SR-INP-16).
#include "input/input_manager.hpp"

#include <fcntl.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

namespace arcade {
namespace {

/// Most digits accepted after "event". Not an SRS value: a guard so the number fits in a long.
constexpr std::size_t kMaxEventDigits = 9;

/// Event number of a `/dev/input` entry named "event<N>" (`len` characters), or -1 for any
/// other name (SR-INP-01). Does not allocate.
long eventNumber(const char* name, std::size_t len) {
    const std::size_t prefixLen = std::strlen(InputManager::kEventPrefix);
    if (len <= prefixLen || std::strncmp(name, InputManager::kEventPrefix, prefixLen) != 0) {
        return -1;
    }
    // Real event numbers are small; longer names are rejected so `number` cannot overflow.
    if (len - prefixLen > kMaxEventDigits) {
        return -1;
    }
    long number = 0;
    for (std::size_t i = prefixLen; i < len; ++i) {
        const char c = name[i];
        if (c < '0' || c > '9') {
            return -1;
        }
        number = number * 10 + (c - '0');
    }
    return number;
}

long eventNumber(const std::string& name) { return eventNumber(name.data(), name.size()); }

/// Entry name of event device `number`: "event<N>" (SR-INP-01).
std::string eventName(long number) {
    return std::string(InputManager::kEventPrefix) + std::to_string(number);
}

bool hasKey(const KeyBits& bits, std::uint16_t code) {
    return (bits[code / 8U] & (1U << (code % 8U))) != 0;
}

}  // namespace

void InputManager::PendingDevices::entryChanged(const char* name) {
    // SR-INP-15: only event devices matter. Keep the name only if it is exactly "event<N>"
    // with no leading zero, so eventName() gives the same name back. Does not allocate.
    const std::size_t len = std::strlen(name);
    const std::size_t prefixLen = std::strlen(kEventPrefix);
    const long number = eventNumber(name, len);
    if (number < 0 || (name[prefixLen] == '0' && len > prefixLen + 1)) {
        return;
    }
    long* const last = numbers_.data() + count_;
    long* const pos = std::lower_bound(numbers_.data(), last, number);
    if (pos != last && *pos == number) {
        return;  // already pending
    }
    if (count_ == numbers_.size()) {
        fullScan_ = true;  // too many changes: scan everything instead
        return;
    }
    std::copy_backward(pos, last, last + 1);  // keep ascending order (SR-INP-01)
    *pos = number;
    ++count_;
}

InputManager::InputManager(EvdevSys& sys) : sys_(sys) {}

InputManager::~InputManager() {
    closeKeyboard();
    if (watchFd_ >= 0) {
        sys_.close(watchFd_);
    }
}

void InputManager::closeKeyboard() {
    if (fd_ >= 0) {
        sys_.close(fd_);  // closing the device also releases the EVIOCGRAB grab
        fd_ = -1;
    }
    path_.clear();
}

bool InputManager::start() {
    state_ = InputState::Scanning;
    // SR-INP-15: watch before scanning, so a keyboard plugged in during the scan is not missed.
    if (watchFd_ < 0) {
        watchFd_ = sys_.watchDir(kInputDir);
    }
    // A repeated scan replaces the current keyboard; close it first so its fd and grab are freed.
    closeKeyboard();
    pressed_.fill(false);
    pending_.clear();
    // SR-INP-16: start-up continues without a keyboard.
    state_ = scan() ? InputState::Ready : InputState::NoKeyboard;
    return state_ == InputState::Ready;
}

bool InputManager::scan() {
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
        if (tryDevice(candidate.second)) {
            return true;
        }
    }
    return false;
}

bool InputManager::tryDevice(const std::string& name) {
    const std::string path = std::string(kInputDir) + "/" + name;
    // SR-INP-02: non-blocking, so pollInput() never waits for a key.
    const int fd = sys_.open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return false;
    }
    // SR-INP-01: skip mice and other devices without the game keys.
    // SR-INP-03: exclusive access; a device another process holds is not used.
    if (!isKeyboard(fd) || !sys_.grab(fd)) {
        sys_.close(fd);
        return false;
    }
    fd_ = fd;
    path_ = path;
    return true;
}

bool InputManager::checkPending() {
    // SR-INP-15: check the devices inotify reported, as in SR-INP-01 (lowest event number
    // first). A device that cannot be opened yet is dropped here; the IN_ATTRIB event that
    // follows when its mode changes reports it again.
    if (pending_.fullScan()) {
        pending_.clear();
        return scan();
    }
    std::array<long, kMaxPendingDevices> numbers{};
    const auto count = static_cast<std::size_t>(pending_.end() - pending_.begin());
    std::copy(pending_.begin(), pending_.end(), numbers.begin());
    pending_.clear();
    for (std::size_t i = 0; i < count; ++i) {
        if (tryDevice(eventName(numbers[i]))) {
            return true;
        }
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

std::size_t InputManager::readBatch(ReadEnd& end) {
    int err = 0;
    const ssize_t bytes = sys_.read(fd_, buf_.data(), sizeof(buf_), err);
    if (bytes <= 0) {
        // SR-INP-04: EAGAIN means the queue is empty for this tick. SR-INP-12: ENODEV means the
        // keyboard was removed. Other errors only end the tick.
        if (bytes < 0 && err == ENODEV) {
            end = ReadEnd::DeviceGone;
        } else if (bytes < 0 && err == EAGAIN) {
            end = ReadEnd::Empty;
        } else {
            end = ReadEnd::Error;
        }
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

void InputManager::enterDisconnected() {
    // SR-INP-12: the stale file descriptor is closed on entry to Disconnected.
    closeKeyboard();
    // SR-INP-13: a key held while unplugging must not stay pressed.
    pressed_.fill(false);
    state_ = InputState::Disconnected;
}

void InputManager::readWatch() {
    if (watchFd_ < 0) {
        // The watch could not be set up at start-up; try again, and scan everything once if it
        // now works, since changes before it existed were not reported (SR-INP-15).
        watchFd_ = sys_.watchDir(kInputDir);
        if (watchFd_ < 0) {
            return;
        }
        pending_.requestFullScan();
    }
    sys_.readDirChanges(watchFd_, pending_);
}

}  // namespace arcade
