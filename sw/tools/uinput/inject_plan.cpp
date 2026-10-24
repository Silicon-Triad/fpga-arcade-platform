// Timing and keys of a PR-06 injection run.
#include "tools/uinput/inject_plan.hpp"

namespace arcade::uinput {
namespace {

using std::chrono::nanoseconds;
using std::chrono::seconds;

/// Time between two presses: 1 s / keysPerSecond.
nanoseconds period(const InjectPlan& plan) {
    return nanoseconds(seconds(1)) / static_cast<nanoseconds::rep>(plan.keysPerSecond);
}

}  // namespace

std::uint16_t keyForPair(std::size_t i) { return kInjectKeys[i % kInjectKeys.size()]; }

nanoseconds pressAt(const InjectPlan& plan, std::size_t i) {
    // Computed from i, not added up, so rounding does not drift over 1,800 pairs.
    return nanoseconds(seconds(1)) * static_cast<nanoseconds::rep>(i) /
           static_cast<nanoseconds::rep>(plan.keysPerSecond);
}

nanoseconds releaseAt(const InjectPlan& plan, std::size_t i) {
    return pressAt(plan, i) + period(plan) / 2;
}

nanoseconds planDuration(const InjectPlan& plan) {
    return plan.pairs == 0 ? nanoseconds(0) : releaseAt(plan, plan.pairs - 1);
}

}  // namespace arcade::uinput
