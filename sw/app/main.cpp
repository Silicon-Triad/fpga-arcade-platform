// The arcade program: chooses the backend and runs the start-up sequence (SR-FW-01, SR-FW-05).
#include <csignal>
#include <cstdio>
#include <memory>
#include <string_view>
#include <utility>

#include "app/options.hpp"
#include "app/startup.hpp"

#ifdef ARCADE_HAS_SDL
#include "app/binding_lookup.hpp"
#include "backend_sdl/font_text_layout.hpp"
#include "backend_sdl/sdl_backend.hpp"
#endif

namespace {

using arcade::BackendKind;
using arcade::BackendStart;

constexpr int kExitUsage = 2;
constexpr std::string_view kStepNoFpga = "FPGA backend is not available yet";

// SR-FW-14: the handler only sets this; the game loop acts on it between ticks. The one global a
// handler needs: it can reach nothing else (async-signal-safe).
volatile std::sig_atomic_t gStopSignal = 0;

extern "C" void onStopSignal(int /*signal*/) { gStopSignal = 1; }

#ifdef ARCADE_HAS_SDL
constexpr std::string_view kStepOpenSdl = "open SDL2 window";
constexpr BackendKind kDefaultBackend = BackendKind::Sdl;  // SR-FW-05: the PC build
// DESIGN CHOICE, NOT AN SRS VALUE: 3 s, so that screen 10b can be read before the window closes.
constexpr int kSdlFinalFrameTicks = 3 * arcade::TICK_RATE;
#else
constexpr std::string_view kStepNoSdl = "SDL2 backend is not in this build";
constexpr BackendKind kDefaultBackend = BackendKind::Fpga;
#endif

BackendStart startBackend(const arcade::Options& options) {
    BackendStart start;
    if (options.backend == BackendKind::Fpga) {
        start.failedStep = kStepNoFpga;  // SF-02: until the FPGA backend exists (#37)
        return start;
    }
#ifdef ARCADE_HAS_SDL
    // Static: BindingLookup keeps a reference to `bindings`, and the backend to `lookup` and
    // `textLayout`, until the program ends.
    static arcade::KeyBindings bindings;
    static const arcade::BindingLookup lookup(bindings);
    static const arcade::FontTextLayout textLayout;
    // SC-05: ignored lines keep their defaults; say so, so that a typo is not silent.
    const arcade::KeymapLoadResult keymap = bindings.loadOverrideFile();
    if (keymap.ignored > 0) {
        (void)std::fprintf(stderr, "warning: %.*s: %zu line(s) ignored, first at line %zu\n",
                           static_cast<int>(arcade::KeyBindings::kKeymapFile.size()),
                           arcade::KeyBindings::kKeymapFile.data(), keymap.ignored,
                           keymap.firstIgnoredLine);
    }
    std::unique_ptr<arcade::SdlBackend> sdl = arcade::SdlBackend::open(lookup, &textLayout);
    if (sdl == nullptr) {
        start.failedStep = kStepOpenSdl;
        return start;
    }
    const arcade::SdlBackend* window = sdl.get();
    start.stopRequested = [window] { return window->quitRequested(); };
    start.finalFrameTicks = kSdlFinalFrameTicks;
    start.api = std::move(sdl);
#else
    start.failedStep = kStepNoSdl;
#endif
    return start;
}

}  // namespace

int main(int argc, char** argv) {
    const auto options = arcade::parseOptions(argc, argv, kDefaultBackend);
    if (!options) {
        (void)std::fprintf(stderr, "usage: arcade [--backend=sdl|fpga] [--profile]\n");
        return kExitUsage;
    }
    // Before the backend starts: SDL then leaves these handlers in place.
    (void)std::signal(SIGINT, &onStopSignal);   // SR-FW-14
    (void)std::signal(SIGTERM, &onStopSignal);  // SR-FW-14
    return arcade::runArcade(*options, &startBackend, stderr, [] { return gStopSignal != 0; });
}
