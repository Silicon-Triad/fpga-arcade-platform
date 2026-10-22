// Unit tests for the Input Manager on recorded devices and events (issue #20,
// SR-INP-01 to SR-INP-08).
#include "input/input_manager.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>

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
/// batch of events returned by one read() call; an empty queue gives EAGAIN.
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
    int readCalls = 0;

    void addDevice(const std::string& name, const KeyBits& bits) {
        names.push_back(name);
        devices["/dev/input/" + name].bits = bits;
    }
    Device& dev(const std::string& name) { return devices.at("/dev/input/" + name); }

    std::vector<std::string> listDir(const std::string& dir) override {
        return dir == "/dev/input" ? names : std::vector<std::string>{};
    }
    int open(const std::string& path, int flags) override {
        openFlags.push_back(flags);
        if (devices.count(path) == 0 || devices.at(path).openFails) {
            return -1;
        }
        const int fd = nextFd_++;
        open_[fd] = path;
        return fd;
    }
    void close(int fd) override { open_.erase(fd); }
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
    // ENODEV (keyboard removed) is only handled by #22; until then it must end the tick quietly,
    // keeping the events already read.
    reads().push_back({keyEvent(KEY_A, 1)});
    sys.dev("event0").errorWhenEmpty = ENODEV;
    const auto out = pollAll(im);
    ASSERT_EQ(out.size(), 1U);
    EXPECT_TRUE(isDown(out[0], KEY_A));
    EXPECT_TRUE(pollAll(im).empty());
    EXPECT_TRUE(im.isPressed(KEY_A));
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
    presses.reserve(KEY_CNT);
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

}  // namespace
}  // namespace arcade
