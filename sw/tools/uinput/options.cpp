// Command-line options of the uinput tools.
#include "tools/uinput/options.hpp"

#include <cerrno>
#include <cstdlib>
#include <limits>

namespace arcade::uinput {
namespace {

/// Parses a whole decimal number into `out`. No exceptions (R-4.1.5).
bool parseNumber(const char* text, unsigned long long max, unsigned long long& out) {
    if (text == nullptr || *text < '0' || *text > '9') {
        return false;  // also rejects "-1" and "+1"
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (errno != 0 || *end != '\0' || value > max) {
        return false;
    }
    out = value;
    return true;
}

}  // namespace

bool parseOptions(int argc, const char* const* argv, Options& out, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            out.help = true;
            continue;
        }
        if (arg != "--pairs" && arg != "--rate" && arg != "--device" && arg != "--delay") {
            error = "unknown option " + arg;
            return false;
        }
        if (i + 1 >= argc) {
            error = arg + " needs a value";
            return false;
        }
        const char* value = argv[++i];
        unsigned long long n = 0;
        if (arg == "--device") {
            out.device = value;
        } else if (arg == "--pairs") {
            if (!parseNumber(value, std::numeric_limits<std::size_t>::max(), n)) {
                error = "--pairs needs a whole number";
                return false;
            }
            out.plan.pairs = static_cast<std::size_t>(n);
        } else if (arg == "--rate") {
            if (!parseNumber(value, std::numeric_limits<unsigned>::max(), n) || n == 0) {
                error = "--rate needs a whole number of at least 1";
                return false;
            }
            out.plan.keysPerSecond = static_cast<unsigned>(n);
        } else {
            if (!parseNumber(value, std::numeric_limits<unsigned>::max(), n)) {
                error = "--delay needs a whole number of seconds";
                return false;
            }
            out.delaySeconds = static_cast<unsigned>(n);
        }
    }
    return true;
}

std::string usage(const std::string& name) {
    return "usage: " + name +
           " [--pairs N] [--rate KEYS_PER_SECOND] [--device PATH] [--delay SECONDS]\n"
           "  --pairs   press/release pairs to inject (default " +
           std::to_string(kPr06Pairs) + ", PR-06)\n" +
           "  --rate    key presses per second (default " + std::to_string(kPr06KeysPerSecond) +
           ", PR-06)\n" + "  --device  uinput device (default " + kDefaultUinputPath + ")\n" +
           "  --delay   uinput_inject only: seconds to wait before the first key (default 3)\n";
}

}  // namespace arcade::uinput
