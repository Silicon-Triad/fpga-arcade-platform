#include "app/startup.hpp"

#include <cinttypes>
#include <optional>

#include "arcade/tables.hpp"
#include "assets/game_tiles.hpp"
#include "assets/palette.hpp"
#include "assets/tile_set.hpp"
#include "engine/app.hpp"
#include "engine/game_loop.hpp"
#include "engine/screens/boot_screen.hpp"
#include "engine/tick_clock.hpp"
#include "games/game_registry.hpp"

namespace arcade {
namespace {

// DESIGN CHOICE, NOT AN SRS VALUE: the splash shows for 1 s so that the player can read it; the
// bar fills over that time. Far inside BOOT_LIMIT (SR-FW-01).
constexpr int kSplashTicks = TICK_RATE;

// Issue #76 supplies the real lookup.
std::uint32_t noBestScore(std::uint8_t) { return 0; }

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

    App app(api, kGameRegistry.data(), kGameRegistry.size(), &noBestScore);
    MonotonicClock clock;
    GameLoop loop(api, clock, GameLoopConfig{options.profile});

    // Section 3.1.1 screen 1, then the menu (SR-FW-01). App has no Boot-to-Menu transition, so
    // the splash runs here. A keyboard check belongs to the FPGA backend (#37): SDL always has one.
    BootScreen boot;
    loop.setScreen(&boot);
    for (int tick = 1; tick <= kSplashTicks && !stop(); ++tick) {
        boot.setProgress(tick, kSplashTicks);
        loop.runTick();
    }
    app.setState(AppState::Menu);
    loop.setScreen(&app);
    loop.run(stop);

    if (options.profile) {
        printProfile(loop.report(), log);
    }
    return 0;
}

}  // namespace arcade
