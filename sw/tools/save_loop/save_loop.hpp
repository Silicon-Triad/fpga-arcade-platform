/// @file
/// SF-04 power-cut test helpers: two complete reference tables written in turn with the real
/// atomic save, and a check that a file holds exactly one of them (SF-04, SR-FW-09, #45).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "engine/file_system.hpp"
#include "engine/high_score_store.hpp"

namespace arcade::save_loop {

/// Which of the two reference tables. Each has HISCORE_SLOTS valid entries per game (SF-04).
enum class Pattern : std::uint8_t { A, B };

/// The letter printed for a pattern: 'A' or 'B'.
char patternName(Pattern pattern) noexcept;

/// A store filled with reference table `pattern`: HISCORE_SLOTS entries for every game, initials
/// "AAA" or "BBB", scores that differ between A and B (SF-04: "10 valid entries per game").
HighScoreStore makeStore(Pattern pattern) noexcept;

/// What `checkFile` found in the high-score file after a power cut (SF-04).
struct CheckResult {
    bool fileFound = false;         ///< The file exists and could be read (SR-FW-08).
    std::size_t rejectedLines = 0;  ///< Non-empty lines that fail validation (SC-04).
    std::array<std::size_t, kHighScoreGameCount> entries{};  ///< Valid entries per game.
    std::optional<Pattern> match;  ///< Set if the loaded table equals A or B exactly.
};

/// Reads `path` and compares it with both reference tables (SF-04, SC-04).
CheckResult checkFile(FileSystem& fs, std::string_view path);

/// SF-04 pass: the file loads, no line is rejected, every game has HISCORE_SLOTS entries and the
/// whole table is either A or B, so it is the complete old or the complete new table.
bool passed(const CheckResult& result) noexcept;

/// One-line report of `result`, ending in PASS or FAIL.
std::string report(const CheckResult& result);

/// What the program was asked to do.
enum class Mode : std::uint8_t { None, Write, Check };

/// Command-line options of `save_loop`.
struct Options {
    Mode mode = Mode::None;          ///< First argument: `write` or `check`.
    std::string file{HISCORE_FILE};  ///< --file PATH (default HISCORE_FILE, SR-FW-08).
    std::uint64_t count = 0;         ///< --count N saves for `write`; 0 runs until killed.
    bool help = false;               ///< --help
};

/// Parses `write|check [--file PATH] [--count N] [--help]`. Returns false and sets `error` on
/// anything else. No exceptions (R-4.1.5).
bool parseOptions(int argc, const char* const* argv, Options& out, std::string& error);

/// Usage text for program `name`.
std::string usage(const std::string& name);

}  // namespace arcade::save_loop
