#include "app/startup.hpp"

#include <cinttypes>
#include <optional>

#include "arcade/tables.hpp"
#include "assets/game_tiles.hpp"
#include "assets/palette.hpp"
#include "assets/tile_set.hpp"
#include "engine/app.hpp"
#include "engine/file_system.hpp"
#include "engine/game_loop.hpp"
#include "engine/high_score_store.hpp"
#include "engine/tick_clock.hpp"
#include "games/game_registry.hpp"

namespace arcade {
namespace {

// DESIGN CHOICE, NOT AN SRS VALUE: the splash shows for 1 s so that the player can read it; the
// bar fills over that time. Far inside BOOT_LIMIT (SR-FW-01).
constexpr int kSplashTicks = TICK_RATE;

// The high-score table the menu reads its BEST line from (SR-FW-08, SR-UI-02). Loaded once at
// start-up; BestScoreFn is a plain function pointer, so the lookup reaches it through here.
const HighScoreStore* gBestScores = nullptr;

// SR-UI-02: the best score is rank 1 of the game's table, or 0 while the table is empty.
std::uint32_t bestScoreOf(std::uint8_t gameId) {
    if (gBestScores == nullptr) {
        return 0;
    }
    const auto& table = gBestScores->table(gameId);
    return table.count > 0 ? table.rows[0].score : 0;
}

int fail(std::FILE* log, std::string_view step) {
    // SF-02: the message and the failed step, then a non-zero exit.
    (void)std::fprintf(log, "%.*s: %.*s\n", static_cast<int>(kHardwareNotReady.size()),
                       kHardwareNotReady.data(), static_cast<int>(step.size()), step.data());
    return kExitHardwareNotReady;
}

void printProfile(const GameLoopReport& report, std::FILE* log) {
    (void)std::fprintf(log,
                       "ticks %" PRIu64 " frames %u present_errors %" PRIu64 " p99.9_us %" PRIu64
                       " over_budget %" PRIu64 " max_us %" PRIu64 "\n",
                       report.ticks, report.frameCountEnd - report.frameCountStart,
                       report.presentErrors, report.tickStats.p999Us, report.tickStats.overBudget,
                       report.tickStats.maxUs);
}

}  // namespace

int runArcade(const Options& options, const BackendFactory& factory, std::FILE* log) {
    // SR-FW-01: start the backend, then load tiles and palette before anything is presented.
    BackendStart backend = factory(options);
    if (backend.api == nullptr) {
        return fail(log, backend.failedStep);
    }
    EngineApi& api = *backend.api;
    const auto stop = [&backend] { return backend.stopRequested && backend.stopRequested(); };

    const auto& gameTiles = assets::gameTilePatterns();
    const std::optional<TileSet> tiles = assets::buildTileSet(gameTiles.data(), gameTiles.size());
    if (!tiles) {
        return fail(log, kStepBuildTiles);
    }
    if (api.loadTiles(*tiles, assets::kPalette) != ApiStatus::Ok) {
        return fail(log, kStepLoadTiles);
    }

    // SR-FW-08: a missing or unreadable HISCORE_FILE gives an empty table, never a failure.
    static HighScoreStore scores;
    PosixFileSystem fs;
    scores.load(fs);
    gBestScores = &scores;

    App app(api, kGameRegistry.data(), kGameRegistry.size(), &bestScoreOf);
    MonotonicClock clock;
    GameLoop loop(api, clock, GameLoopConfig{options.profile});

    // Section 3.1.1 screen 1, then the menu (SR-FW-01). A keyboard check belongs to the FPGA
    // backend (#37): SDL always has one.
    loop.setScreen(&app);
    for (int tick = 1; tick <= kSplashTicks && !stop(); ++tick) {
        app.boot().setProgress(tick, kSplashTicks);
        loop.runTick();
    }
    app.bootFinished();
    loop.run(stop);

    if (options.profile) {
        printProfile(loop.report(), log);
    }
    return 0;
}

}  // namespace arcade
