#include "app/options.hpp"

#include <string_view>

namespace arcade {

std::optional<Options> parseOptions(int argc, const char* const* argv,
                                    BackendKind defaultBackend) noexcept {
    Options options;
    options.backend = defaultBackend;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--backend=sdl") {  // SR-FW-05
            options.backend = BackendKind::Sdl;
        } else if (arg == "--backend=fpga") {  // SR-FW-05
            options.backend = BackendKind::Fpga;
        } else if (arg == "--profile") {
            options.profile = true;
        } else {
            return std::nullopt;
        }
    }
    return options;
}

}  // namespace arcade
