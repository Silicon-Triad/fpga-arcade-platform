// SF-04 power-cut test helpers (#45).
#include "tools/save_loop/save_loop.hpp"

#include <cerrno>
#include <cstdlib>

namespace arcade::save_loop {
namespace {

/// Not SRS values: chosen so A and B differ in every row and stay below SCORE_MAX.
constexpr std::uint32_t kScoreStep = 1'000;
constexpr std::uint32_t kPatternBOffset = 500;

bool sameRow(const HighScoreRow& a, const HighScoreRow& b) noexcept {
    return a.game_id == b.game_id && a.rank == b.rank && a.initials == b.initials &&
           a.score == b.score && a.seq == b.seq;
}

bool sameTables(const HighScoreStore& a, const HighScoreStore& b) noexcept {
    for (std::uint8_t game = 0; game < kHighScoreGameCount; ++game) {
        const auto& ta = a.table(game);
        const auto& tb = b.table(game);
        if (ta.count != tb.count) {
            return false;
        }
        for (std::size_t i = 0; i < ta.count; ++i) {
            if (!sameRow(ta.rows[i], tb.rows[i])) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace

char patternName(Pattern pattern) noexcept { return pattern == Pattern::A ? 'A' : 'B'; }

HighScoreStore makeStore(Pattern pattern) noexcept {
    HighScoreStore store;
    const char letter = patternName(pattern);
    const std::array<char, INITIALS_LEN> initials{letter, letter, letter};
    const std::uint32_t offset = pattern == Pattern::A ? 0 : kPatternBOffset;
    for (std::uint8_t game = 0; game < kHighScoreGameCount; ++game) {
        for (std::uint32_t i = 0; i < HISCORE_SLOTS; ++i) {
            // SR-FW-10: insert() assigns rank and seq; scores fall with i, so rank = i + 1.
            (void)store.insert(game, initials,
                               kScoreStep * static_cast<std::uint32_t>(HISCORE_SLOTS - i) + offset);
        }
    }
    return store;
}

CheckResult checkFile(FileSystem& fs, std::string_view path) {
    CheckResult result;
    const std::optional<std::string> text = fs.readAll(std::string(path));
    if (!text) {
        return result;
    }
    result.fileFound = true;
    // SC-04: count the lines load() would drop, split the same way as HighScoreStore::load.
    std::string_view rest = *text;
    while (!rest.empty()) {
        const std::size_t end = rest.find('\n');
        const std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        if (!line.empty() && !HighScoreStore::parseLine(line)) {
            ++result.rejectedLines;
        }
    }
    HighScoreStore loaded;
    loaded.load(fs, path);
    for (std::uint8_t game = 0; game < kHighScoreGameCount; ++game) {
        result.entries[game] = loaded.table(game).count;
    }
    for (const Pattern p : {Pattern::A, Pattern::B}) {
        if (sameTables(loaded, makeStore(p))) {
            result.match = p;
        }
    }
    return result;
}

bool passed(const CheckResult& result) noexcept {
    if (!result.fileFound || result.rejectedLines != 0 || !result.match) {
        return false;
    }
    for (const std::size_t n : result.entries) {
        if (n != HISCORE_SLOTS) {
            return false;
        }
    }
    return true;
}

std::string report(const CheckResult& result) {
    if (!result.fileFound) {
        return "file missing or unreadable -> FAIL";
    }
    std::string out = "table=";
    out += result.match ? std::string(1, patternName(*result.match)) : std::string("neither");
    for (std::size_t game = 0; game < result.entries.size(); ++game) {
        out += " game" + std::to_string(game) + "=" + std::to_string(result.entries[game]);
    }
    out += " rejected=" + std::to_string(result.rejectedLines);
    out += passed(result) ? " -> PASS" : " -> FAIL";
    return out;
}

bool parseOptions(int argc, const char* const* argv, Options& out, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            out.help = true;
        } else if (arg == "write" || arg == "check") {
            if (out.mode != Mode::None) {
                error = "give write or check only once";
                return false;
            }
            out.mode = arg == "write" ? Mode::Write : Mode::Check;
        } else if (arg == "--file" || arg == "--count") {
            if (i + 1 >= argc) {
                error = arg + " needs a value";
                return false;
            }
            const char* value = argv[++i];
            if (arg == "--file") {
                out.file = value;
                if (out.file.empty()) {
                    error = "--file needs a path";
                    return false;
                }
                continue;
            }
            char* end = nullptr;
            errno = 0;
            const unsigned long long n = std::strtoull(value, &end, 10);
            if (*value < '0' || *value > '9' || errno != 0 || *end != '\0') {
                error = "--count needs a whole number";
                return false;
            }
            out.count = n;
        } else {
            error = "unknown argument " + arg;
            return false;
        }
    }
    if (!out.help && out.mode == Mode::None) {
        error = "say write or check";
        return false;
    }
    return true;
}

std::string usage(const std::string& name) {
    return "usage: " + name +
           " write|check [--file PATH] [--count N]\n"
           "  write  save reference tables A and B in turn with the real atomic save (SR-FW-09)\n"
           "         until killed or power is cut, or N saves with --count N\n"
           "  check  after a reboot: the file must hold exactly table A or table B with 10\n"
           "         valid entries per game and no rejected line (SF-04); prints PASS or FAIL\n"
           "  --file PATH  high-score file (default " +
           std::string(HISCORE_FILE) + ")\n";
}

}  // namespace arcade::save_loop
