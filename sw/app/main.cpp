// The arcade program: chooses the backend and runs the start-up sequence (SR-FW-01, SR-FW-05).
#include <csignal>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "app/bridge_open.hpp"
#include "app/fpga_start.hpp"
#include "app/options.hpp"
#include "app/startup.hpp"
#include "hal/tilemap_hal.hpp"
#include "input/evdev_sys.hpp"
#include "input/input_manager.hpp"
#include "input/key_bindings.hpp"

#ifdef ARCADE_HAS_SDL
#include "app/binding_lookup.hpp"
#include "backend_sdl/font_text_layout.hpp"
#include "backend_sdl/sdl_backend.hpp"
#endif

namespace {

using arcade::BackendKind;
using arcade::BackendStart;

constexpr int kExitUsage = 2;

// SR-FW-14: the handler sets this; the game loop acts on it between ticks. The one global a
// handler needs: it can reach nothing else (async-signal-safe).
volatile std::sig_atomic_t gStopSignal = 0;

// A second signal ends the program at once, so a hung save can still be stopped. signal() is
// async-signal-safe.
extern "C" void onStopSignal(int /*signal*/) {
    gStopSignal = 1;
    (void)std::signal(SIGINT, SIG_DFL);
    (void)std::signal(SIGTERM, SIG_DFL);
}

#ifdef ARCADE_HAS_SDL
constexpr std::string_view kStepOpenSdl = "open SDL2 window";
constexpr BackendKind kDefaultBackend = BackendKind::Sdl;  // SR-FW-05: the PC build
// DESIGN CHOICE, NOT AN SRS VALUE: 3 s, so that screen 10b can be read before the window closes.
constexpr int kSdlFinalFrameTicks = 3 * arcade::TICK_RATE;
#else
constexpr std::string_view kStepNoSdl = "SDL2 backend is not in this build";
constexpr BackendKind kDefaultBackend = BackendKind::Fpga;
#endif

// TBL-KeyBinding with keymap.conf applied (SC-05).
arcade::KeyBindings loadKeyBindings() {
    arcade::KeyBindings bindings;
    // SC-05: ignored lines keep their defaults; say so, so that a typo is not silent.
    const arcade::KeymapLoadResult keymap = bindings.loadOverrideFile();
    if (keymap.ignored > 0) {
        (void)std::fprintf(stderr, "warning: %.*s: %zu line(s) ignored, first at line %zu\n",
                           static_cast<int>(arcade::KeyBindings::kKeymapFile.size()),
                           arcade::KeyBindings::kKeymapFile.data(), keymap.ignored,
                           keymap.firstIgnoredLine);
    }
    return bindings;
}

// Loaded once, on first use. Static: the backends keep a reference to it until the program ends.
const arcade::KeyBindings& keyBindings() {
    static const arcade::KeyBindings bindings = loadKeyBindings();
    return bindings;
}

// The mapped and checked bridge window (SR-HAL-01, SR-HAL-02, SR-HAL-09). #34 replaces this body
// with its /dev/mem RegisterSpace and readiness check; nothing else here changes.
arcade::BridgeOpen openBridge() {
    // Static: the backend keeps a reference to the space until the program ends.
    static arcade::hal::LinuxBridgeSys sys;
    static arcade::hal::DevMemRegisterSpace space(sys, arcade::hal::kBoardReadiness);
    static std::string failedStep;  // SF-02: the failed step, printed by runArcade()
    return arcade::openCheckedBridge(space, failedStep);
}

BackendStart startBackend(const arcade::Options& options) {
    BackendStart start;
    if (options.backend == BackendKind::Fpga) {
        // Static: the backend keeps references to these until the program ends.
        static arcade::hal::SteadyClock clock;
        static arcade::LinuxEvdevSys evdev;
        static arcade::InputManager input(evdev);
        return arcade::startFpgaBackend(openBridge(), clock, input, keyBindings());
    }
#ifdef ARCADE_HAS_SDL
    // Static: the backend keeps references to `lookup` and `textLayout` until the program ends.
    static const arcade::BindingLookup lookup(keyBindings());
    static const arcade::FontTextLayout textLayout;
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
