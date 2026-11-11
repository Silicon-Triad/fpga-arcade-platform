/// @file
/// A one-keyboard fake of EvdevSys for FpgaBackend and start-up tests (SR-INP-01 to SR-INP-16).
/// The full recorded-device fake is in sw/tests/input/input_manager_test.cpp.
#pragma once

#include <gtest/gtest.h>
#include <linux/input.h>

#include <cerrno>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#include "input/evdev_sys.hpp"
#include "input/input_manager.hpp"

namespace arcade::test {

/// /dev/input with at most one keyboard, `event0`. Each queued read is one read() batch; an
/// empty queue gives EAGAIN, an unplugged keyboard ENODEV (SR-INP-04, SR-INP-12).
class FakeEvdevSys final : public EvdevSys {
  public:
    static constexpr int kKeyboardFd = 10;
    static constexpr int kWatchFd = 11;

    bool present = true;  ///< `event0` exists and is a keyboard.
    bool unplugged = false;
    bool goneWhenEmpty = false;  ///< Once the queued reads are used up, read() gives ENODEV.
    std::deque<std::vector<input_event>> reads;
    std::deque<std::string> dirEvents;  ///< Queued inotify entry names (SR-INP-15).
    int readCalls = 0;
    int eagainReads = 0;  ///< Reads that returned EAGAIN: the queue was empty (SR-INP-04).

    void key(std::uint16_t code, std::int32_t value) {
        input_event ev{};
        ev.type = EV_KEY;
        ev.code = code;
        ev.value = value;
        reads.push_back({ev});
    }
    void press(std::uint16_t code) { key(code, InputManager::kValuePress); }
    void release(std::uint16_t code) { key(code, InputManager::kValueRelease); }
    /// The keyboard is removed: its reads give ENODEV (SR-INP-12).
    void unplug() {
        unplugged = true;
        present = false;
        reads.clear();
    }
    /// A keyboard is plugged in as `event0` and inotify reports it (SR-INP-15).
    void plugIn() {
        unplugged = false;
        present = true;
        dirEvents.emplace_back("event0");
    }

    std::vector<std::string> listDir(const std::string& /*dir*/) override {
        return present ? std::vector<std::string>{"event0"} : std::vector<std::string>{};
    }
    int open(const std::string& path, int /*flags*/) override {
        return present && path == "/dev/input/event0" ? kKeyboardFd : -1;
    }
    void close(int /*fd*/) override {}
    bool keyBits(int /*fd*/, KeyBits& bits) override {
        bits = {};
        for (const std::uint16_t c : InputManager::kRequiredKeys) {
            bits[c / 8U] = static_cast<std::uint8_t>(bits[c / 8U] | (1U << (c % 8U)));
        }
        return true;
    }
    bool grab(int /*fd*/) override { return true; }
    ssize_t read(int /*fd*/, void* buf, std::size_t len, int& err) override {
        ++readCalls;
        if (unplugged || (goneWhenEmpty && reads.empty())) {
            err = ENODEV;
            return -1;
        }
        if (reads.empty()) {
            ++eagainReads;
            err = EAGAIN;
            return -1;
        }
        const std::vector<input_event> batch = reads.front();  // tests queue small batches
        reads.pop_front();
        EXPECT_LE(batch.size() * sizeof(input_event), len);
        std::memcpy(buf, batch.data(), batch.size() * sizeof(input_event));
        err = 0;
        return static_cast<ssize_t>(batch.size() * sizeof(input_event));
    }
    int watchDir(const std::string& /*dir*/) override { return kWatchFd; }
    void readDirChanges(int /*watchFd*/, DirChangeSink& sink) override {
        while (!dirEvents.empty()) {
            const std::string name = dirEvents.front();
            dirEvents.pop_front();
            sink.entryChanged(name.c_str());
        }
    }
};

}  // namespace arcade::test
