// Comparison of reported key events with the injected ones (PR-06).
#include "tools/uinput/throughput_check.hpp"

namespace arcade::uinput {

void ThroughputCheck::fail(const std::string& what) {
    if (errors_ == 0) {
        firstError_ = what;
    }
    ++errors_;
}

void ThroughputCheck::keyDown(std::uint16_t code) {
    ++downs_;
    if (next_ >= plan_.pairs) {
        fail("extra KeyDown " + std::to_string(code) + " after the last pair");
    } else if (held_) {
        fail("KeyDown " + std::to_string(code) + " before the KeyUp of pair " +
             std::to_string(next_));
    } else if (code != keyForPair(next_)) {
        fail("pair " + std::to_string(next_) + ": KeyDown " + std::to_string(code) + ", expected " +
             std::to_string(keyForPair(next_)));
    } else {
        held_ = true;
    }
}

void ThroughputCheck::keyUp(std::uint16_t code) {
    ++ups_;
    if (!held_) {
        fail("KeyUp " + std::to_string(code) + " without its KeyDown (pair " +
             std::to_string(next_) + ")");
    } else if (code != keyForPair(next_)) {
        fail("pair " + std::to_string(next_) + ": KeyUp " + std::to_string(code) + ", expected " +
             std::to_string(keyForPair(next_)));
    } else {
        held_ = false;
        ++next_;
    }
}

void ThroughputCheck::otherEvent() { fail("unexpected non-key event"); }

}  // namespace arcade::uinput
