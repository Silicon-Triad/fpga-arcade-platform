#pragma once

#include <linux/input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "input/evdev_sys.hpp"

namespace arcade {

/// Kind of key event the Input Manager produces (SR-INP-05, SR-INP-06).
enum class KeyEventType : std::uint8_t { KeyDown, KeyUp };

/// One key press or release, carrying the Linux key code (`KEY_*`) (SR-INP-05, SR-INP-06).
struct KeyEvent {
    KeyEventType type = KeyEventType::KeyDown;
    std::uint16_t code = 0;
};

/// Reads the USB keyboard through evdev and turns its events into KeyDown and KeyUp events,
/// keeping a pressed/released state for every key (SR-INP-01 to SR-INP-08).
///
/// Key bindings (SR-INP-09 to SR-INP-11) and hot-plug (SR-INP-12 to SR-INP-16) are issues #21
/// and #22.
class InputManager {
  public:
    /// SR-INP-01: directory and name prefix of the event devices that are scanned.
    static constexpr const char* kInputDir = "/dev/input";
    static constexpr const char* kEventPrefix = "event";

    /// SR-INP-01: keys a device must report to be selected as the keyboard:
    /// KEY_A to KEY_Z, KEY_ENTER, KEY_ESC and the four arrow keys.
    static constexpr std::array<std::uint16_t, 32> kRequiredKeys = {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E,     KEY_F,   KEY_G,  KEY_H,    KEY_I,    KEY_J,    KEY_K,
        KEY_L, KEY_M, KEY_N, KEY_O, KEY_P,     KEY_Q,   KEY_R,  KEY_S,    KEY_T,    KEY_U,    KEY_V,
        KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_ENTER, KEY_ESC, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT};

    /// SR-INP-05 to SR-INP-07: `input_event.value` meanings for `EV_KEY`.
    static constexpr std::int32_t kValueRelease = 0;
    static constexpr std::int32_t kValuePress = 1;
    static constexpr std::int32_t kValueRepeat = 2;

    /// Uses `sys` for every system call; `sys` must outlive this object.
    explicit InputManager(EvdevSys& sys);
    ~InputManager();
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;
    InputManager(InputManager&&) = delete;
    InputManager& operator=(InputManager&&) = delete;

    /// Start-up scan: selects the first `/dev/input/event*` device (in event-number order) that
    /// reports every key in kRequiredKeys (SR-INP-01), opens it with `O_NONBLOCK` (SR-INP-02)
    /// and grabs it with `EVIOCGRAB` (SR-INP-03). A device that cannot be grabbed is not used.
    /// Returns true if a keyboard was selected.
    bool start();

    /// True while a keyboard is selected and open (SR-INP-01).
    bool hasKeyboard() const { return fd_ >= 0; }

    /// Path of the selected keyboard, or empty if none (SR-INP-01).
    const std::string& devicePath() const { return path_; }

    /// Called once per game tick. Reads every queued event until `read()` returns `EAGAIN`
    /// (SR-INP-04), updates the key state (SR-INP-08) and calls `sink(const KeyEvent&)` for each
    /// KeyDown (SR-INP-05) and KeyUp (SR-INP-06), in arrival order. Auto-repeat (value 2) and
    /// non-key events are dropped (SR-INP-07). Never blocks (SR-INP-02) and never allocates.
    template <typename Sink>
    void poll(Sink&& sink);

    /// True if key `code` is held: its last event was a KeyDown (SR-INP-08). False for codes
    /// outside the `KEY_*` range.
    bool isPressed(std::uint16_t code) const { return code < KEY_CNT && pressed_[code]; }

  private:
    /// Events read per `read()` call. Not an SRS value: poll() repeats the call until `EAGAIN`,
    /// so this does not limit how many events one tick returns (SR-INP-04).
    static constexpr std::size_t kReadBatch = 64;

    bool isKeyboard(int fd);
    std::size_t readBatch();
    bool translate(const input_event& raw, KeyEvent& out);

    EvdevSys& sys_;
    int fd_ = -1;
    std::string path_;
    std::array<input_event, kReadBatch> buf_{};
    std::array<bool, KEY_CNT> pressed_{};  // SR-INP-08: one entry per key code
};

template <typename Sink>
void InputManager::poll(Sink&& sink) {
    if (fd_ < 0) {
        return;
    }
    // SR-INP-04: drain the queue; readBatch() returns 0 on EAGAIN (or any other error).
    for (std::size_t n = readBatch(); n > 0; n = readBatch()) {
        for (std::size_t i = 0; i < n; ++i) {
            KeyEvent ev;
            if (translate(buf_[i], ev)) {
                sink(ev);
            }
        }
    }
}

}  // namespace arcade
