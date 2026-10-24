#pragma once

#include <linux/input.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "input/evdev_sys.hpp"

namespace arcade {

/// Kind of event the Input Manager produces: a key press or release (SR-INP-05, SR-INP-06), or
/// the keyboard being lost or found again (SR-INP-14, SR-INP-15).
enum class KeyEventType : std::uint8_t { KeyDown, KeyUp, KeyboardLost, KeyboardFound };

/// Input Manager state (App. A im_state, Input Manager state machine in SRS 3.3,
/// SR-INP-12 to SR-INP-16).
enum class InputState : std::uint8_t {
    NoKeyboard,    ///< No keyboard was found at start-up (SR-INP-16).
    Scanning,      ///< Start-up scan not finished (SR-INP-01).
    Ready,         ///< A keyboard is open and grabbed (SR-INP-01 to SR-INP-03).
    Disconnected,  ///< The keyboard was removed (SR-INP-12).
};

/// One Input Manager event. For KeyDown and KeyUp, `code` is the Linux key code (`KEY_*`)
/// (SR-INP-05, SR-INP-06); for KeyboardLost and KeyboardFound it is 0 (SR-INP-14, SR-INP-15).
struct KeyEvent {
    KeyEventType type = KeyEventType::KeyDown;
    std::uint16_t code = 0;
};

/// Reads the USB keyboard through evdev and turns its events into KeyDown and KeyUp events,
/// keeping a pressed/released state for every key (SR-INP-01 to SR-INP-08). Handles the keyboard
/// being unplugged and plugged in again (SR-INP-12 to SR-INP-16).
///
/// Key bindings (SR-INP-09 to SR-INP-11) are issue #21.
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
    /// Also starts watching `/dev/input` with inotify for hot-plug (SR-INP-15). Enters Ready, or
    /// NoKeyboard if no keyboard was found (SR-INP-16). Returns true if a keyboard was selected.
    bool start();

    /// Current state (App. A im_state). Scanning until start() has run.
    InputState state() const { return state_; }

    /// True while a keyboard is selected and open (SR-INP-01).
    bool hasKeyboard() const { return fd_ >= 0; }

    /// Path of the selected keyboard, or empty if none (SR-INP-01).
    const std::string& devicePath() const { return path_; }

    /// Called once per game tick. Reads every queued event until `read()` returns `EAGAIN`
    /// (SR-INP-04), updates the key state (SR-INP-08) and calls `sink(const KeyEvent&)` for each
    /// KeyDown (SR-INP-05) and KeyUp (SR-INP-06), in arrival order. Auto-repeat (value 2) and
    /// non-key events are dropped (SR-INP-07).
    ///
    /// Hot-plug: if `read()` returns `ENODEV`, the keyboard is closed, the state becomes
    /// Disconnected (SR-INP-12), every key is set to released (SR-INP-13) and KeyboardLost is
    /// passed to `sink` after the events already read, in the same call (SR-INP-14). inotify
    /// is read in every state, and the `eventN` devices it reports are remembered. In the
    /// Disconnected and NoKeyboard states those devices, and only those, are checked as in
    /// SR-INP-01; a keyboard found enters Ready and gives KeyboardFound (SR-INP-15). Its keys
    /// are read from the next call on. Other devices are not opened again, because opening some
    /// of them can take hundreds of milliseconds (sw/input/README.md).
    ///
    /// Never blocks (SR-INP-02). Allocates only while checking devices after a hot-plug change
    /// (SR-INP-15), never while a keyboard is Ready.
    template <typename Sink>
    void poll(Sink&& sink);

    /// True if key `code` is held: its last event was a KeyDown (SR-INP-08). False for codes
    /// outside the `KEY_*` range.
    bool isPressed(std::uint16_t code) const { return code < KEY_CNT && pressed_[code]; }

  private:
    /// Events read per `read()` call. Not an SRS value: poll() repeats the call until `EAGAIN`,
    /// so this does not limit how many events one tick returns (SR-INP-04).
    static constexpr std::size_t kReadBatch = 64;

    /// Why readBatch() returned no events.
    enum class ReadEnd : std::uint8_t {
        Empty,       ///< `EAGAIN`: the queue is drained for this tick (SR-INP-04).
        DeviceGone,  ///< `ENODEV`: the keyboard was removed (SR-INP-12).
        Error,       ///< Any other error: ends the tick, the keyboard is kept.
    };

    /// Most devices remembered between checks. Not an SRS value: if more change before the
    /// next check, or the kernel drops inotify events, the next check is a full scan instead.
    static constexpr std::size_t kMaxPendingDevices = 16;

    /// `eventN` devices reported by inotify and not yet checked (SR-INP-15). Fixed size, so
    /// reading inotify never allocates.
    class PendingDevices final : public DirChangeSink {
      public:
        void entryChanged(const char* name) override;
        void eventsLost() override { fullScan_ = true; }

        /// Next check scans the whole directory (the watch was missing, events were lost).
        void requestFullScan() { fullScan_ = true; }
        bool empty() const { return !fullScan_ && count_ == 0; }
        bool fullScan() const { return fullScan_; }
        /// Event numbers in ascending order (SR-INP-01 order).
        const long* begin() const { return numbers_.data(); }
        const long* end() const { return numbers_.data() + count_; }
        void clear() {
            count_ = 0;
            fullScan_ = false;
        }

      private:
        std::array<long, kMaxPendingDevices> numbers_{};
        std::size_t count_ = 0;
        bool fullScan_ = false;
    };

    bool scan();
    bool tryDevice(const std::string& name);
    bool checkPending();
    bool isKeyboard(int fd);
    std::size_t readBatch(ReadEnd& end);
    bool translate(const input_event& raw, KeyEvent& out);
    void enterDisconnected();
    void readWatch();
    void closeKeyboard();

    EvdevSys& sys_;
    InputState state_ = InputState::Scanning;
    int fd_ = -1;
    int watchFd_ = -1;  // inotify on kInputDir, or -1 (SR-INP-15)
    PendingDevices pending_;
    std::string path_;
    std::array<input_event, kReadBatch> buf_{};
    std::array<bool, KEY_CNT> pressed_{};  // SR-INP-08: one entry per key code
};

template <typename Sink>
void InputManager::poll(Sink&& sink) {
    if (state_ == InputState::Ready) {
        // SR-INP-04: drain the queue; readBatch() returns 0 once it is empty or on an error.
        ReadEnd end = ReadEnd::Empty;
        for (std::size_t n = readBatch(end); n > 0; n = readBatch(end)) {
            for (std::size_t i = 0; i < n; ++i) {
                KeyEvent ev;
                if (translate(buf_[i], ev)) {
                    sink(ev);
                }
            }
        }
        if (end == ReadEnd::DeviceGone) {
            enterDisconnected();                            // SR-INP-12, SR-INP-13
            sink(KeyEvent{KeyEventType::KeyboardLost, 0});  // SR-INP-14: same tick
        }
    }
    // SR-INP-15: read inotify in every state. Devices that appear while Ready are kept, so a
    // second keyboard, or one plugged in again before ENODEV was read, is found at once.
    readWatch();
    if ((state_ == InputState::Disconnected || state_ == InputState::NoKeyboard) &&
        !pending_.empty()) {
        if (checkPending()) {
            state_ = InputState::Ready;
            sink(KeyEvent{KeyEventType::KeyboardFound, 0});  // SR-INP-15
        }
    }
}

}  // namespace arcade
