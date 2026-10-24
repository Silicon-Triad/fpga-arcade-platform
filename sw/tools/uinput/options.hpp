#pragma once

#include <string>

#include "tools/uinput/inject_plan.hpp"
#include "tools/uinput/uinput_device.hpp"

namespace arcade::uinput {

/// Command-line options shared by `uinput_inject` and `input_throughput_test`.
struct Options {
    InjectPlan plan;                          ///< --pairs, --rate (defaults: PR-06)
    std::string device = kDefaultUinputPath;  ///< --device
    unsigned delaySeconds = 3;                ///< --delay: uinput_inject only. Not an SRS value.
    bool help = false;                        ///< --help
};

/// Parses `--pairs N`, `--rate N` (keys per second, at least 1), `--device PATH`,
/// `--delay N` and `--help`. Returns false and sets `error` on anything else.
bool parseOptions(int argc, const char* const* argv, Options& out, std::string& error);

/// One-paragraph usage text for program `name`.
std::string usage(const std::string& name);

}  // namespace arcade::uinput
