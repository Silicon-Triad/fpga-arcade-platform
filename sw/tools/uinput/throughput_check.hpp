#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "tools/uinput/inject_plan.hpp"

namespace arcade::uinput {

/// Checks what the Input Manager reported against an InjectPlan (PR-06): every press and
/// release, none lost, none extra, in the order they were injected.
class ThroughputCheck {
  public:
    explicit ThroughputCheck(const InjectPlan& plan) : plan_(plan) {}

    /// A KeyDown with key code `code` was reported.
    void keyDown(std::uint16_t code);
    /// A KeyUp with key code `code` was reported.
    void keyUp(std::uint16_t code);
    /// Any other event was reported (for example KeyboardLost).
    void otherEvent();

    std::size_t keyDowns() const { return downs_; }
    std::size_t keyUps() const { return ups_; }
    std::size_t errors() const { return errors_; }
    /// The first problem found, or empty if none.
    const std::string& firstError() const { return firstError_; }

    /// PR-06: exactly `pairs` KeyDown and `pairs` KeyUp, in plan order, and nothing else.
    bool passed() const { return downs_ == plan_.pairs && ups_ == plan_.pairs && errors_ == 0; }

  private:
    void fail(const std::string& what);

    InjectPlan plan_;
    std::size_t downs_ = 0;
    std::size_t ups_ = 0;
    std::size_t errors_ = 0;
    std::size_t next_ = 0;  // index of the pair expected next
    bool held_ = false;     // the press of pair next_ was seen, its release not yet
    std::string firstError_;
};

}  // namespace arcade::uinput
