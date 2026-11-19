// save_loop: the SF-04 test program (#45). See sw/tools/save_loop/README.md.
#include <cinttypes>
#include <cstdio>
#include <string>

#include "engine/file_system.hpp"
#include "tools/save_loop/save_loop.hpp"

namespace {

/// Not an SRS value: how often `write` prints progress.
constexpr std::uint64_t kReportEvery = 100;

int runWrite(const arcade::save_loop::Options& options) {
    using arcade::save_loop::Pattern;
    arcade::PosixFileSystem fs;
    std::array<arcade::HighScoreStore, 2> stores{arcade::save_loop::makeStore(Pattern::A),
                                                 arcade::save_loop::makeStore(Pattern::B)};
    std::printf("writing tables A and B in turn to %s; cut power at any time\n",
                options.file.c_str());
    (void)std::fflush(stdout);
    for (std::uint64_t n = 0; options.count == 0 || n < options.count; ++n) {
        // SR-FW-09: the real save: temporary file, fsync(), rename().
        const auto status = stores[n % 2].save(fs, options.file);
        if (status != arcade::SaveStatus::Ok) {
            std::printf("save %" PRIu64 " failed (status %d) -> FAIL\n", n + 1,
                        static_cast<int>(status));
            return 1;
        }
        if ((n + 1) % kReportEvery == 0) {
            std::printf("saves %" PRIu64 ", last table %c\n", n + 1, n % 2 == 0 ? 'A' : 'B');
            (void)std::fflush(stdout);
        }
    }
    std::printf("done, %" PRIu64 " saves\n", options.count);
    return 0;
}

int runCheck(const arcade::save_loop::Options& options) {
    arcade::PosixFileSystem fs;
    const auto result = arcade::save_loop::checkFile(fs, options.file);
    std::printf("%s: %s\n", options.file.c_str(), arcade::save_loop::report(result).c_str());
    return arcade::save_loop::passed(result) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    arcade::save_loop::Options options;
    std::string error;
    if (!arcade::save_loop::parseOptions(argc, argv, options, error)) {
        (void)std::fprintf(stderr, "save_loop: %s\n%s", error.c_str(),
                           arcade::save_loop::usage("save_loop").c_str());
        return 2;
    }
    if (options.help) {
        std::printf("%s", arcade::save_loop::usage("save_loop").c_str());
        return 0;
    }
    return options.mode == arcade::save_loop::Mode::Write ? runWrite(options) : runCheck(options);
}
