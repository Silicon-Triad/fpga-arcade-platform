// Manual demo of the SDL2 backend (issue #17, SR-FW-06): opens the window, loads a few test
// tiles and a palette, draws a pattern and prints the action of each key press. Esc or
// closing the window quits. Run: ./build/pc-debug/sw/tools/sdl_demo
#include <array>
#include <cstdio>
#include <map>

#include "backend_sdl/sdl_backend.hpp"

namespace {

using arcade::Action;
using arcade::InputContext;
namespace key = arcade::linux_key;

/// DEMO ONLY, NOT TBL-KeyBinding: a few keys so presses print something. The real table and
/// lookup are issue #21.
class DemoLookup : public arcade::KeyActionLookup {
  public:
    std::optional<arcade::KeyMapping> lookup(InputContext /*context*/,
                                             std::uint16_t keyCode) const noexcept override {
        static const std::map<std::uint16_t, arcade::KeyMapping> kKeys = {
            {key::kUp, {Action::Up, 0}},        {key::kDown, {Action::Down, 0}},
            {key::kLeft, {Action::Left, 0}},    {key::kRight, {Action::Right, 0}},
            {key::kEnter, {Action::Select, 0}}, {key::kEsc, {Action::Back, 0}},
            {key::kM, {Action::Mute, 0}},
        };
        const auto it = kKeys.find(keyCode);
        if (it == kKeys.end()) {
            return std::nullopt;
        }
        return it->second;
    }
};

const char* actionName(Action a) {
    static constexpr std::array<const char*, arcade::kActionCount> kNames = {
        "Up",    "Down",   "Left", "Right",  "Select", "Back",
        "Pause", "Launch", "Char", "Delete", "Mute"};
    return kNames[static_cast<std::size_t>(a)];
}

}  // namespace

int main() {
    DemoLookup lookup;
    auto backend = arcade::SdlBackend::open(lookup);
    if (!backend) {
        (void)std::fprintf(stderr, "sdl_demo: could not open the SDL2 window\n");
        return 1;
    }

    // Test tiles: 0 blank, 1 solid colour 1, 2 checkerboard of 2 and 3, 3 left half 4 / right
    // half 5 (shows the low-nibble-left order of TBL-TilePattern).
    arcade::TileSet tiles{};
    tiles[1].fill(0x11);
    for (std::size_t i = 0; i < arcade::kTilePatternBytes; ++i) {
        const bool oddRow = (i / 4) % 2 == 1;
        tiles[2][i] = oddRow ? 0x23 : 0x32;
        tiles[3][i] = (i % 4) < 2 ? 0x44 : 0x55;
    }
    tiles[4][0] = 0x05;  // only the top-left pixel set: low nibble = left pixel
    const arcade::Palette palette = {0x00000000, 0x00FFFFFF, 0x00FF0000, 0x0000FF00,
                                     0x000000FF, 0x00FFFF00, 0x00FF00FF, 0x0000FFFF,
                                     0x00808080, 0x00800000, 0x00008000, 0x00000080,
                                     0x00808000, 0x00800080, 0x00008080, 0x00C0C0C0};
    if (backend->loadTiles(tiles, palette) != arcade::ApiStatus::Ok) {
        return 1;
    }
    backend->clear(0);
    (void)backend->fillRect(0, 0, arcade::kGridCols, 1, 1);                      // top border
    (void)backend->fillRect(0, arcade::kGridRows - 1, arcade::kGridCols, 1, 1);  // bottom border
    (void)backend->fillRect(8, 8, 32, 16, 2);
    (void)backend->fillRect(48, 8, 32, 16, 3);
    for (int col = 0; col < arcade::kGridCols; col += 2) {
        (void)backend->setTile(col, 40, 4);
    }
    (void)backend->setInputContext(InputContext::Menu);

    (void)std::printf("sdl_demo: press arrows, Enter, Esc or M; Esc or closing the window quits\n");
    bool quit = false;
    while (!quit && !backend->quitRequested()) {
        const arcade::InputFrame frame = backend->pollInput();
        for (std::size_t i = 0; i < frame.count; ++i) {
            const arcade::InputEvent& ev = frame.events[i];
            if (ev.type == arcade::InputEventType::KeyDown) {
                (void)std::printf("frame %u: KeyDown %s\n", backend->frameCount(),
                                  actionName(ev.action));
                quit = quit || ev.action == Action::Back;
            }
        }
        if (backend->present() != arcade::ApiStatus::Ok) {
            (void)std::fprintf(stderr, "sdl_demo: present() failed\n");
            return 1;
        }
    }
    return 0;
}
