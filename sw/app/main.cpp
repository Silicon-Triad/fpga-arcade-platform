// The arcade program: chooses the backend and runs the start-up sequence (SR-FW-01, SR-FW-05).
#include <cstdio>
#include <string_view>

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

#ifdef ARCADE_HAS_SDL
constexpr std::string_view kStepOpenSdl = "open SDL2 window";
constexpr BackendKind kDefaultBackend = BackendKind::Sdl;  // SR-FW-05: the PC build
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
    // Static: the backend keeps references to these until the program ends.
    static arcade::KeyBindings bindings;
    static const arcade::BindingLookup lookup(bindings);
    static const arcade::FontTextLayout textLayout;
    (void)bindings.loadOverrideFile();  // SC-05
    std::unique_ptr<arcade::SdlBackend> sdl = arcade::SdlBackend::open(lookup, &textLayout);
    if (sdl == nullptr) {
        start.failedStep = kStepOpenSdl;
        return start;
    }
    const arcade::SdlBackend* window = sdl.get();
    start.stopRequested = [window] { return window->quitRequested(); };
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
    return arcade::runArcade(*options, &startBackend, stderr);
}
