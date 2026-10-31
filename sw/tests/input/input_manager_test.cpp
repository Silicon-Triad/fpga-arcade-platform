// Unit tests for the Input Manager on recorded devices and events (issues #20 and #22,
// SR-INP-01 to SR-INP-08, SR-INP-12 to SR-INP-16).
#include "input/input_manager.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace arcade {
namespace {

input_event keyEvent(std::uint16_t code, std::int32_t value) {
    input_event ev{};
    ev.type = EV_KEY;
    ev.code = code;
    ev.value = value;
    return ev;
}

input_event synReport() {
    input_event ev{};
    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    return ev;
}

KeyBits bitsFor(const std::vector<std::uint16_t>& codes) {
    KeyBits bits{};
    for (const std::uint16_t c : codes) {
        bits[c / 8U] = static_cast<std::uint8_t>(bits[c / 8U] | (1U << (c % 8U)));
    }
    return bits;
}

KeyBits keyboardBits() {
    const auto& req = InputManager::kRequiredKeys;
    std::vector<std::uint16_t> codes(req.begin(), req.end());
    codes.push_back(KEY_SPACE);
    codes.push_back(KEY_LEFTCTRL);
    return bitsFor(codes);
}

KeyBits mouseBits() { return bitsFor({BTN_LEFT, BTN_RIGHT, BTN_MIDDLE}); }

/// Recorded /dev/input: devices with key capabilities and queued reads. Each queued read is a
/// batch of events returned by one read() call; an empty queue gives EAGAIN. inotify is a queue
/// of entry names (SR-INP-15).
class FakeEvdevSys final : public EvdevSys {
  public:
    struct Device {
        KeyBits bits{};
        bool grabFails = false;
        bool openFails = false;       // open() returns -1
        bool keyBitsFails = false;    // EVIOCGBIT ioctl fails
        int errorWhenEmpty = EAGAIN;  // errno once the queued reads are used up
        std::deque<std::vector<input_event>> reads;
    };

    std::vector<std::string> names;         // directory listing, in this order
    std::map<std::string, Device> devices;  // full path -> device
    std::map<int, std::string> open_;       // fd -> path, currently open
    std::vector<int> openFlags;             // flags of every open() call
    std::vector<std::string> grabbed;       // paths grabbed successfully
    std::vector<int> closed;                // every fd passed to close(), in order
    std::map<std::string, int> opens;       // path -> number of open() calls
    int readCalls = 0;
    int listDirCalls = 0;

    // inotify (SR-INP-15)
    bool watchFails = false;            // watchDir() returns -1
    std::vector<std::string> watched;   // every dir passed to watchDir()
    int watchFd = -1;                   // last fd returned by watchDir()
    std::deque<std::string> dirEvents;  // queued inotify entry names; readDirChanges() takes all
    bool eventsLost = false;            // next readDirChanges() reports IN_Q_OVERFLOW

    void addDevice(const std::string& name, const KeyBits& bits) {
        names.push_back(name);
        devices["/dev/input/" + name].bits = bits;
    }
    Device& dev(const std::string& name) { return devices.at("/dev/input/" + name); }

    /// A device node appears (IN_CREATE). The same name may come back after an unplug.
    void plugIn(const std::string& name, const KeyBits& bits) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            names.push_back(name);
        }
        Device fresh;
        fresh.bits = bits;
        devices["/dev/input/" + name] = fresh;
        dirEvents.push_back(name);
    }
    /// The device is removed: reads on its open fd give ENODEV and its node is gone. Removal is
    /// IN_DELETE, which the Input Manager does not watch, so no inotify event is queued.
    void unplug(const std::string& name) {
        Device& d = dev(name);
        d.reads.clear();
        d.errorWhenEmpty = ENODEV;
        d.openFails = true;
        names.erase(std::remove(names.begin(), names.end(), name), names.end());
    }

    std::vector<std::string> listDir(const std::string& dir) override {
        ++listDirCalls;
        return dir == "/dev/input" ? names : std::vector<std::string>{};
    }
    int open(const std::string& path, int flags) override {
        openFlags.push_back(flags);
        ++opens[path];
        if (devices.count(path) == 0 || devices.at(path).openFails) {
            return -1;
        }
        const int fd = nextFd_++;
        open_[fd] = path;
        return fd;
    }
    void close(int fd) override {
        closed.push_back(fd);
        open_.erase(fd);
    }
    bool keyBits(int fd, KeyBits& bits) override {
        const Device& d = devices.at(open_.at(fd));
        bits = d.bits;
        return !d.keyBitsFails;
    }
    bool grab(int fd) override {
        if (devices.at(open_.at(fd)).grabFails) {
            return false;
        }
        grabbed.push_back(open_.at(fd));
        return true;
    }
    ssize_t read(int fd, void* buf, std::size_t len, int& err) override {
        ++readCalls;
        Device& d = devices.at(open_.at(fd));
        auto& reads = d.reads;
        if (reads.empty()) {
            err = d.errorWhenEmpty;
            return -1;
        }
        std::vector<input_event> batch = reads.front();
        const std::size_t fit = len / sizeof(input_event);
        if (batch.size() > fit) {  // like the kernel: return what fits, keep the rest
            reads.front().erase(reads.front().begin(),
                                reads.front().begin() + static_cast<std::ptrdiff_t>(fit));
            batch.resize(fit);
        } else {
            reads.pop_front();
        }
        std::memcpy(buf, batch.data(), batch.size() * sizeof(input_event));
        err = 0;
        return static_cast<ssize_t>(batch.size() * sizeof(input_event));
    }
    int watchDir(const std::string& dir) override {
        watched.push_back(dir);
        if (watchFails) {
            return -1;
        }
        watchFd = nextFd_++;
        return watchFd;
    }
    void readDirChanges(int fd, DirChangeSink& sink) override {
        EXPECT_EQ(fd, watchFd);
        if (eventsLost) {
            eventsLost = false;
            sink.eventsLost();
        }
        while (!dirEvents.empty()) {
            const std::string name = dirEvents.front();
            dirEvents.pop_front();
            sink.entryChanged(name.c_str());
        }
    }

  private:
    int nextFd_ = 3;
};

std::vector<KeyEvent> pollAll(InputManager& im) {
    std::vector<KeyEvent> out;
    im.poll([&out](const KeyEvent& ev) { out.push_back(ev); });
    return out;
}

bool isDown(const KeyEvent& ev, std::uint16_t code) {
    return ev.type == KeyEventType::KeyDown && ev.code == code;
}
bool isUp(const KeyEvent& ev, std::uint16_t code) {
    return ev.type == KeyEventType::KeyUp && ev.code == code;
}

class InputManagerTest : public ::testing::Test {
  protected:
    FakeEvdevSys sys;
};

// --- SR-INP-01: keyboard selection ---------------------------------------------------------

TEST_F(InputManagerTest, SR_INP_01_KeyboardChosenOverMouse) {
    sys.addDevice("event0", mouseBits());
    sys.addDevice("event1", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.devicePath(), "/dev/input/event1");
    ASSERT_EQ(sys.open_.size(), 1U);  // the mouse was opened, checked and closed again
    EXPECT_EQ(sys.open_.begin()->second, "/dev/input/event1");
    EXPECT_EQ(sys.grabbed, std::vector<std::string>{"/dev/input/event1"});
}

TEST_F(InputManagerTest, SR_INP_01_FirstKeyboardInEventNumberOrder) {
    sys.addDevice("event10", keyboardBits());
    sys.addDevice("event2", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.devicePath(), "/dev/input/event2");
}

TEST_F(InputManagerTest, SR_INP_01_DeviceMissingARequiredKeyIsNotChosen) {
    for (const std::uint16_t missing : InputManager::kRequiredKeys) {
        FakeEvdevSys s;
        const auto& req = InputManager::kRequiredKeys;
        std::vector<std::uint16_t> codes;
        for (const std::uint16_t c : req) {
            if (c != missing) {
                codes.push_back(c);
            }
        }
        s.addDevice("event0", bitsFor(codes));
        InputManager im(s);
        EXPECT_FALSE(im.start()) << "selected without key code " << missing;
        EXPECT_FALSE(im.hasKeyboard());
        EXPECT_TRUE(s.open_.empty());
    }
}

TEST_F(InputManagerTest, SR_INP_01_OnlyEventDevicesAreScanned) {
    sys.names = {"by-id", "mice", "mouse0", "js0", "eventX", "event"};
    sys.devices["/dev/input/mice"].bits = keyboardBits();
    sys.devices["/dev/input/eventX"].bits = keyboardBits();
    InputManager im(sys);
    EXPECT_FALSE(im.start());
    EXPECT_TRUE(sys.openFlags.empty());
}

TEST_F(InputManagerTest, SR_INP_01_NoDevicesMeansNoKeyboard) {
    InputManager im(sys);
    EXPECT_FALSE(im.start());
    EXPECT_FALSE(im.hasKeyboard());
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_EQ(sys.readCalls, 0);
}

TEST_F(InputManagerTest, SR_INP_01_OverlongEventNumberIsIgnored) {
    sys.addDevice("event99999999999999999999", keyboardBits());
    sys.addDevice("event1234567890", keyboardBits());
    InputManager im(sys);
    EXPECT_FALSE(im.start());
    EXPECT_TRUE(sys.openFlags.empty());
}

TEST_F(InputManagerTest, SR_INP_01_StartTwiceClosesThePreviousKeyboard) {
    sys.addDevice("event0", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    ASSERT_TRUE(im.start());
    EXPECT_EQ(sys.open_.size(), 1U);  // the first fd was closed, not leaked
    EXPECT_EQ(im.devicePath(), "/dev/input/event0");
}

TEST_F(InputManagerTest, SR_INP_01_DeviceThatCannotBeOpenedIsSkipped) {
    sys.addDevice("event0", keyboardBits());
    sys.addDevice("event1", keyboardBits());
    sys.dev("event0").openFails = true;
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.devicePath(), "/dev/input/event1");
}

TEST_F(InputManagerTest, SR_INP_01_DeviceWhoseKeyBitsCannotBeReadIsSkipped) {
    sys.addDevice("event0", keyboardBits());
    sys.addDevice("event1", keyboardBits());
    sys.dev("event0").keyBitsFails = true;
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.devicePath(), "/dev/input/event1");
    EXPECT_EQ(sys.open_.size(), 1U);  // event0 was closed again
}

// --- SR-INP-02 and SR-INP-03: open mode and grab ---------------------------------------------

TEST_F(InputManagerTest, SR_INP_02_OpensReadOnlyAndNonBlocking) {
    sys.addDevice("event0", mouseBits());
    sys.addDevice("event1", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    ASSERT_FALSE(sys.openFlags.empty());
    for (const int flags : sys.openFlags) {
        EXPECT_NE(flags & O_NONBLOCK, 0);
        EXPECT_EQ(flags & O_ACCMODE, O_RDONLY);
    }
}

TEST_F(InputManagerTest, SR_INP_03_KeyboardIsGrabbed) {
    sys.addDevice("event0", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(sys.grabbed, std::vector<std::string>{"/dev/input/event0"});
}

TEST_F(InputManagerTest, SR_INP_03_KeyboardThatCannotBeGrabbedIsSkipped) {
    sys.addDevice("event0", keyboardBits());
    sys.addDevice("event1", keyboardBits());
    sys.dev("event0").grabFails = true;
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.devicePath(), "/dev/input/event1");
    EXPECT_EQ(sys.open_.size(), 1U);
}

TEST_F(InputManagerTest, SR_INP_03_DestructorClosesKeyboard) {
    sys.addDevice("event0", keyboardBits());
    {
        InputManager im(sys);
        ASSERT_TRUE(im.start());
        EXPECT_EQ(sys.open_.size(), 1U);
    }
    EXPECT_TRUE(sys.open_.empty());
}

// --- SR-INP-04: drain the queue every tick --------------------------------------------------

class InputManagerReadyTest : public InputManagerTest {
  protected:
    void SetUp() override {
        sys.addDevice("event0", keyboardBits());
        ASSERT_TRUE(im.start());
    }
    std::deque<std::vector<input_event>>& reads() { return sys.dev("event0").reads; }
    InputManager im{sys};
};

TEST_F(InputManagerReadyTest, SR_INP_04_TenEventsInOneTickAllComeBack) {
    // 5 presses and 5 releases, each followed by EV_SYN, delivered by one read().
    const std::array<std::uint16_t, 5> keys = {KEY_A, KEY_LEFT, KEY_RIGHT, KEY_ENTER, KEY_ESC};
    std::vector<input_event> batch;
    for (const std::uint16_t k : keys) {
        batch.push_back(keyEvent(k, 1));
        batch.push_back(synReport());
        batch.push_back(keyEvent(k, 0));
        batch.push_back(synReport());
    }
    reads().push_back(batch);
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 10U);
    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(isDown(out[2 * i], keys[i]));
        EXPECT_TRUE(isUp(out[2 * i + 1], keys[i]));
    }
}

TEST_F(InputManagerReadyTest, SR_INP_04_ReadsAgainUntilEagain) {
    // Ten events spread over ten read() calls: all come back in one tick.
    for (int i = 0; i < 10; ++i) {
        reads().push_back({keyEvent(KEY_SPACE, i % 2 == 0 ? 1 : 0)});
    }
    const int before = sys.readCalls;
    EXPECT_EQ(pollAll(im).size(), 10U);
    EXPECT_EQ(sys.readCalls - before, 11);  // 10 reads with data, then the EAGAIN read
    EXPECT_TRUE(reads().empty());
}

TEST_F(InputManagerReadyTest, SR_INP_04_MoreEventsThanOneReadHolds) {
    // 200 events in the kernel queue, more than one read() buffer: none are lost.
    std::vector<input_event> batch;
    for (int i = 0; i < 100; ++i) {
        batch.push_back(keyEvent(KEY_UP, 1));
        batch.push_back(keyEvent(KEY_UP, 0));
    }
    reads().push_back(batch);
    EXPECT_EQ(pollAll(im).size(), 200U);
}

TEST_F(InputManagerReadyTest, SR_INP_04_EmptyQueueReturnsNothing) {
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_FALSE(im.isPressed(KEY_A));
}

TEST_F(InputManagerReadyTest, SR_INP_04_LaterEventsComeNextTick) {
    reads().push_back({keyEvent(KEY_A, 1)});
    EXPECT_EQ(pollAll(im).size(), 1U);
    reads().push_back({keyEvent(KEY_A, 0)});
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isUp(out[0], KEY_A));
}

TEST_F(InputManagerReadyTest, SR_INP_04_OtherReadErrorEndsTheTick) {
    // An error other than EAGAIN and ENODEV ends the tick quietly, keeping the events already
    // read and the keyboard. (ENODEV is the unplug path, SR-INP-12, tested below.)
    reads().push_back({keyEvent(KEY_A, 1)});
    sys.dev("event0").errorWhenEmpty = EIO;
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isDown(out[0], KEY_A));
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_TRUE(im.isPressed(KEY_A));
    EXPECT_EQ(im.state(), InputState::Ready);
    EXPECT_TRUE(im.hasKeyboard());
}

// --- SR-INP-05 to SR-INP-07: event conversion ----------------------------------------------

TEST_F(InputManagerReadyTest, SR_INP_05_Value1GivesKeyDown) {
    reads().push_back({keyEvent(KEY_LEFT, 1)});
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isDown(out[0], KEY_LEFT));
}

TEST_F(InputManagerReadyTest, SR_INP_06_Value0GivesKeyUp) {
    reads().push_back({keyEvent(KEY_LEFT, 0)});
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isUp(out[0], KEY_LEFT));
}

TEST_F(InputManagerReadyTest, SR_INP_07_Value2IsDropped) {
    // A key held for a while: one press, many auto-repeats, one release.
    std::vector<input_event> batch = {keyEvent(KEY_RIGHT, 1), synReport()};
    for (int i = 0; i < 60; ++i) {
        batch.push_back(keyEvent(KEY_RIGHT, 2));
        batch.push_back(synReport());
    }
    batch.push_back(keyEvent(KEY_RIGHT, 0));
    reads().push_back(batch);
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_TRUE(isDown(out[0], KEY_RIGHT));
    EXPECT_TRUE(isUp(out[1], KEY_RIGHT));
}

TEST_F(InputManagerReadyTest, SR_INP_07_OnlyRepeatsInATickGiveNothing) {
    reads().push_back({keyEvent(KEY_RIGHT, 1)});
    pollAll(im);
    reads().push_back({keyEvent(KEY_RIGHT, 2), keyEvent(KEY_RIGHT, 2)});
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_TRUE(im.isPressed(KEY_RIGHT));
}

TEST_F(InputManagerReadyTest, SR_INP_04_NonKeyEventsAreIgnored) {
    input_event msc{};
    msc.type = EV_MSC;
    msc.code = MSC_SCAN;
    msc.value = 0x70004;
    input_event led{};
    led.type = EV_LED;
    led.code = LED_CAPSL;
    led.value = 1;
    reads().push_back({msc, synReport(), led});
    EXPECT_TRUE(pollAll(im).empty());
}

// --- SR-INP-08: held state --------------------------------------------------------------------

TEST_F(InputManagerReadyTest, SR_INP_08_HeldStateTracked) {
    EXPECT_FALSE(im.isPressed(KEY_LEFT));
    reads().push_back({keyEvent(KEY_LEFT, 1), keyEvent(KEY_SPACE, 1)});
    pollAll(im);
    EXPECT_TRUE(im.isPressed(KEY_LEFT));
    EXPECT_TRUE(im.isPressed(KEY_SPACE));
    EXPECT_FALSE(im.isPressed(KEY_RIGHT));

    pollAll(im);  // nothing new: keys stay held across ticks
    EXPECT_TRUE(im.isPressed(KEY_LEFT));

    reads().push_back({keyEvent(KEY_LEFT, 0)});
    pollAll(im);
    EXPECT_FALSE(im.isPressed(KEY_LEFT));
    EXPECT_TRUE(im.isPressed(KEY_SPACE));
}

TEST_F(InputManagerReadyTest, SR_INP_08_EveryKeyCodeHasAState) {
    std::vector<input_event> presses;
    for (std::uint16_t code = 0; code < KEY_CNT; ++code) {
        presses.push_back(keyEvent(code, 1));
    }
    reads().push_back(presses);
    EXPECT_EQ(pollAll(im).size(), static_cast<std::size_t>(KEY_CNT));
    for (std::uint16_t code = 0; code < KEY_CNT; ++code) {
        EXPECT_TRUE(im.isPressed(code)) << code;
    }
}

TEST_F(InputManagerReadyTest, SR_INP_08_OutOfRangeCodeIsIgnored) {
    reads().push_back({keyEvent(KEY_CNT, 1), keyEvent(0xFFFF, 1)});
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_FALSE(im.isPressed(KEY_CNT));
    EXPECT_FALSE(im.isPressed(0xFFFF));
}

// --- SR-INP-12 to SR-INP-16: hot-plug -------------------------------------------------------

bool isLost(const KeyEvent& ev) { return ev.type == KeyEventType::KeyboardLost && ev.code == 0; }
bool isFound(const KeyEvent& ev) { return ev.type == KeyEventType::KeyboardFound && ev.code == 0; }

TEST_F(InputManagerTest, SR_INP_16_StateIsScanningUntilStart) {
    InputManager im(sys);
    EXPECT_EQ(im.state(), InputState::Scanning);
}

TEST_F(InputManagerTest, SR_INP_16_NoKeyboardAtStartUpEntersNoKeyboard) {
    sys.addDevice("event0", mouseBits());
    InputManager im(sys);
    EXPECT_FALSE(im.start());
    EXPECT_EQ(im.state(), InputState::NoKeyboard);
    EXPECT_FALSE(im.hasKeyboard());
    EXPECT_TRUE(pollAll(im).empty());  // start-up goes on; no KeyboardLost without a keyboard
    EXPECT_EQ(im.state(), InputState::NoKeyboard);
}

TEST_F(InputManagerTest, SR_INP_16_KeyboardAtStartUpEntersReady) {
    sys.addDevice("event0", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    EXPECT_EQ(im.state(), InputState::Ready);
}

TEST_F(InputManagerTest, SR_INP_15_StartWatchesDevInputOnce) {
    sys.addDevice("event0", keyboardBits());
    InputManager im(sys);
    ASSERT_TRUE(im.start());
    ASSERT_TRUE(im.start());
    EXPECT_EQ(sys.watched, std::vector<std::string>{"/dev/input"});
}

TEST_F(InputManagerTest, SR_INP_15_DestructorClosesTheWatch) {
    {
        InputManager im(sys);
        im.start();
    }
    ASSERT_GE(sys.watchFd, 0);
    EXPECT_NE(std::find(sys.closed.begin(), sys.closed.end(), sys.watchFd), sys.closed.end());
}

TEST_F(InputManagerReadyTest, SR_INP_12_EnodevEntersDisconnected) {
    sys.unplug("event0");
    pollAll(im);
    EXPECT_EQ(im.state(), InputState::Disconnected);
    EXPECT_FALSE(im.hasKeyboard());
    EXPECT_TRUE(im.devicePath().empty());
    EXPECT_TRUE(sys.open_.empty());  // the stale fd is closed (SR-INP-12 rationale)
}

TEST_F(InputManagerReadyTest, SR_INP_13_EveryKeyIsReleased) {
    reads().push_back({keyEvent(KEY_LEFT, 1), keyEvent(KEY_A, 1), keyEvent(KEY_SPACE, 1)});
    pollAll(im);
    ASSERT_TRUE(im.isPressed(KEY_LEFT));
    sys.unplug("event0");
    pollAll(im);
    for (std::uint16_t code = 0; code < KEY_CNT; ++code) {
        EXPECT_FALSE(im.isPressed(code)) << code;
    }
}

TEST_F(InputManagerReadyTest, SR_INP_14_KeyboardLostInTheSameTick) {
    // The last events before the unplug come first, then KeyboardLost, all in one poll.
    sys.unplug("event0");
    reads().push_back({keyEvent(KEY_A, 1)});  // still queued when the device went away
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_TRUE(isDown(out[0], KEY_A));
    EXPECT_TRUE(isLost(out[1]));
    EXPECT_TRUE(pollAll(im).empty());  // reported once
}

// Acceptance check of #22, part 1: ENODEV gives the Disconnected state, all keys released and
// KeyboardLost in the same tick.
TEST_F(InputManagerReadyTest, SR_INP_12_13_14_UnplugWhileHoldingAKey) {
    reads().push_back({keyEvent(KEY_RIGHT, 1)});
    pollAll(im);
    ASSERT_TRUE(im.isPressed(KEY_RIGHT));
    sys.unplug("event0");
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isLost(out[0]));
    EXPECT_EQ(im.state(), InputState::Disconnected);
    EXPECT_FALSE(im.isPressed(KEY_RIGHT));
}

TEST_F(InputManagerReadyTest, SR_INP_12_NoReadsWhileDisconnected) {
    sys.unplug("event0");
    pollAll(im);
    const int reads = sys.readCalls;
    pollAll(im);
    pollAll(im);
    EXPECT_EQ(sys.readCalls, reads);
}

// Acceptance check of #22, part 2: a new device is picked up.
TEST_F(InputManagerReadyTest, SR_INP_15_NewKeyboardPickedUpAfterUnplug) {
    sys.unplug("event0");
    pollAll(im);
    EXPECT_TRUE(pollAll(im).empty());  // nothing plugged in yet
    EXPECT_EQ(im.state(), InputState::Disconnected);

    sys.plugIn("event3", keyboardBits());
    const auto out = pollAll(im);  // the next tick after the inotify event, well within 1 s
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(im.state(), InputState::Ready);
    EXPECT_EQ(im.devicePath(), "/dev/input/event3");
    EXPECT_EQ(sys.grabbed.back(), "/dev/input/event3");      // SR-INP-03 again
    EXPECT_EQ(sys.openFlags.back(), O_RDONLY | O_NONBLOCK);  // SR-INP-02 again

    sys.dev("event3").reads.push_back({keyEvent(KEY_UP, 1)});
    const auto keys = pollAll(im);
    ASSERT_EQ(keys.size(), 1U);
    EXPECT_TRUE(isDown(keys[0], KEY_UP));
}

TEST_F(InputManagerReadyTest, SR_INP_15_SameNodeNamePluggedInAgain) {
    sys.unplug("event0");
    pollAll(im);
    sys.plugIn("event0", keyboardBits());
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(im.devicePath(), "/dev/input/event0");
}

TEST_F(InputManagerTest, SR_INP_15_KeyboardPluggedInAfterStartUpWithoutOne) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    EXPECT_TRUE(pollAll(im).empty());
    sys.plugIn("event1", keyboardBits());
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(im.state(), InputState::Ready);
}

TEST_F(InputManagerTest, SR_INP_15_MouseIsNotSelected) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.plugIn("event1", mouseBits());
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_EQ(im.state(), InputState::NoKeyboard);
    EXPECT_TRUE(sys.open_.empty());  // the mouse was checked and closed
    sys.plugIn("event2", keyboardBits());
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(im.devicePath(), "/dev/input/event2");
}

TEST_F(InputManagerTest, SR_INP_15_NoScanWithoutAnInotifyEvent) {
    // inotify drives the search; the directory is not listed every tick.
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    const int lists = sys.listDirCalls;
    sys.addDevice("event1", keyboardBits());  // appears without an inotify event
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(pollAll(im).empty());
    }
    EXPECT_EQ(sys.listDirCalls, lists);
}

TEST_F(InputManagerTest, SR_INP_15_NodeNotYetOpenableIsPickedUpOnAttributeChange) {
    // The node can appear (IN_CREATE) before its group and mode let the `arcade` user open it;
    // the later IN_ATTRIB event starts another scan (sw/input/README.md).
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.plugIn("event1", keyboardBits());
    sys.dev("event1").openFails = true;
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_EQ(im.state(), InputState::NoKeyboard);
    sys.dev("event1").openFails = false;
    sys.dirEvents.emplace_back("event1");  // IN_ATTRIB
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
}

TEST_F(InputManagerReadyTest, SR_INP_15_ChangesWhileReadyDoNotRescan) {
    const int lists = sys.listDirCalls;
    sys.plugIn("event1", keyboardBits());
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_EQ(sys.listDirCalls, lists);
    EXPECT_EQ(im.devicePath(), "/dev/input/event0");
    EXPECT_TRUE(sys.dirEvents.empty());  // but the inotify queue is still drained
}

TEST_F(InputManagerReadyTest, SR_INP_15_OtherKeyboardAlreadyPresentIsTakenAtOnce) {
    // A second keyboard plugged in during play (its inotify event read while Ready), then the
    // first one pulled: the second is selected in the same tick, after KeyboardLost.
    sys.plugIn("event1", keyboardBits());
    pollAll(im);
    sys.unplug("event0");
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_TRUE(isLost(out[0]));
    EXPECT_TRUE(isFound(out[1]));
    EXPECT_EQ(im.devicePath(), "/dev/input/event1");
    EXPECT_EQ(im.state(), InputState::Ready);
}

TEST_F(InputManagerTest, SR_INP_15_WatchThatFailedAtStartUpIsRetried) {
    sys.watchFails = true;
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.addDevice("event1", keyboardBits());
    EXPECT_TRUE(pollAll(im).empty());  // no watch yet, so nothing is reported
    sys.watchFails = false;
    const auto out = pollAll(im);  // the watch works now: scan once for what was missed
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
}

TEST_F(InputManagerReadyTest, SR_INP_12_15_TwentyUnplugCyclesLeakNothing) {
    // Supports SF-03 (20 unplug/replug cycles, a board test in #49): every cycle gives one
    // KeyboardLost and one KeyboardFound, and only one keyboard fd stays open.
    for (int cycle = 0; cycle < 20; ++cycle) {
        const std::string name = (cycle % 2 == 0) ? "event0" : "event1";
        const std::string next = (cycle % 2 == 0) ? "event1" : "event0";
        sys.dev(name).reads.push_back({keyEvent(KEY_LEFT, 1)});
        pollAll(im);
        ASSERT_TRUE(im.isPressed(KEY_LEFT));
        sys.unplug(name);
        auto lost = pollAll(im);
        ASSERT_EQ(lost.size(), 1U) << cycle;
        EXPECT_TRUE(isLost(lost[0]));
        EXPECT_FALSE(im.isPressed(KEY_LEFT));
        sys.plugIn(next, keyboardBits());
        auto found = pollAll(im);
        ASSERT_EQ(found.size(), 1U) << cycle;
        EXPECT_TRUE(isFound(found[0]));
        EXPECT_EQ(im.devicePath(), "/dev/input/" + next);
        EXPECT_EQ(sys.open_.size(), 1U);
    }
}

TEST_F(InputManagerTest, SR_INP_15_OnlyReportedDevicesAreOpened) {
    // Opening some devices is slow (a laptop's event11 took 650 ms per open()). After start-up,
    // a hot-plug change opens only the device inotify names, never the others again.
    sys.addDevice("event0", mouseBits());
    sys.addDevice("event11", mouseBits());
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    ASSERT_EQ(sys.opens["/dev/input/event11"], 1);
    const int lists = sys.listDirCalls;
    sys.plugIn("event21", keyboardBits());
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(sys.opens["/dev/input/event0"], 1);
    EXPECT_EQ(sys.opens["/dev/input/event11"], 1);
    EXPECT_EQ(sys.listDirCalls, lists);
}

TEST_F(InputManagerReadyTest, SR_INP_15_UnplugOpensNoOtherDevice) {
    sys.addDevice("event11", mouseBits());  // already there, not reported again
    sys.unplug("event0");
    const int lists = sys.listDirCalls;
    pollAll(im);
    EXPECT_EQ(im.state(), InputState::Disconnected);
    EXPECT_EQ(sys.opens.count("/dev/input/event11"), 0U);
    EXPECT_EQ(sys.listDirCalls, lists);
}

TEST_F(InputManagerTest, SR_INP_15_EntriesThatAreNotEventDevicesAreIgnored) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.addDevice("event1", keyboardBits());  // its name is not "event1" below, so not tried
    for (const char* name :
         {"by-id", "by-path", "mouse0", "mice", "event", "event01", "eventX", "event1234567890"}) {
        sys.dirEvents.emplace_back(name);
    }
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_EQ(im.state(), InputState::NoKeyboard);
    EXPECT_TRUE(sys.opens.empty() || sys.opens.count("/dev/input/event1") == 0);
}

TEST_F(InputManagerTest, SR_INP_15_ReportedDevicesAreTriedInEventNumberOrder) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.plugIn("event12", keyboardBits());
    sys.plugIn("event3", keyboardBits());
    sys.dirEvents.emplace_back("event12");  // reported twice: tried once
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_EQ(im.devicePath(), "/dev/input/event3");  // SR-INP-01: lowest number first
    EXPECT_EQ(sys.opens.count("/dev/input/event12"), 0U);
}

TEST_F(InputManagerTest, SR_INP_15_LostInotifyEventsGiveOneFullScan) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.addDevice("event4", keyboardBits());  // its event was among those lost
    sys.eventsLost = true;
    const int lists = sys.listDirCalls;
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isFound(out[0]));
    EXPECT_EQ(sys.listDirCalls, lists + 1);
}

TEST_F(InputManagerReadyTest, SR_INP_15_TooManyChangesGiveOneFullScan) {
    // More devices change while Ready than are remembered: the next check scans everything.
    for (int i = 0; i < 20; ++i) {
        sys.plugIn("event" + std::to_string(30 + i), mouseBits());
    }
    pollAll(im);
    sys.addDevice("event5", keyboardBits());  // present, its own event not remembered
    sys.unplug("event0");
    const int lists = sys.listDirCalls;
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 2U);
    EXPECT_TRUE(isLost(out[0]));
    EXPECT_TRUE(isFound(out[1]));
    EXPECT_EQ(im.devicePath(), "/dev/input/event5");
    EXPECT_EQ(sys.listDirCalls, lists + 1);
    pollAll(im);
    EXPECT_EQ(sys.listDirCalls, lists + 1);  // only once
}

TEST_F(InputManagerTest, SR_INP_15_StartForgetsEarlierChanges) {
    InputManager im(sys);
    ASSERT_FALSE(im.start());
    sys.dirEvents.emplace_back("event7");
    pollAll(im);  // tried and dropped (no such device)
    sys.addDevice("event7", keyboardBits());
    EXPECT_TRUE(pollAll(im).empty());  // not reported again, so not tried again
    ASSERT_TRUE(im.start());           // a full scan finds it
}

}  // namespace
}  // namespace arcade
