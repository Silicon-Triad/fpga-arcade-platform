// Unit tests for options, start-up order, SF-02 and screen 1 (issue #16, SR-FW-01, SR-FW-05,
// SF-02, section 3.1.1 screen 1).
#include "app/startup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "app/options.hpp"
#include "assets/game_tiles.hpp"
#include "assets/text_tiles.hpp"
#include "engine/screens/boot_screen.hpp"

namespace arcade {
namespace {

using Tilemap = std::array<std::array<TileIndex, kGridCols>, kGridRows>;

constexpr auto kSf02Limit = std::chrono::seconds(5);  // SF-02

// Records the call order; draws text with the real layout so tests see a true tilemap.
class RecordingApi final : public EngineApi {
  public:
    explicit RecordingApi(std::vector<std::string>& calls, ApiStatus loadStatus = ApiStatus::Ok)
        : calls_(calls), loadStatus_(loadStatus) {}
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        map[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = tile;
        return ApiStatus::Ok;
    }
    ApiStatus fillRect(int col, int row, int width, int height, TileIndex tile) noexcept override {
        for (int r = row; r < row + height; ++r) {
            for (int c = col; c < col + width; ++c) {
                setTile(c, r, tile);
            }
        }
        return ApiStatus::Ok;
    }
    ApiStatus drawText(int col, int row, std::string_view text, TextScale scale) noexcept override {
        return assets::textToTiles(
            col, row, text, scale,
            [](void* self, int c, int r, TileIndex tile) noexcept {
                static_cast<RecordingApi*>(self)->setTile(c, r, tile);
            },
            this);
    }
    void clear(TileIndex tile) noexcept override {
        for (auto& line : map) {
            line.fill(tile);
        }
    }
    ApiStatus present() noexcept override {
        calls_.emplace_back("present");
        return ApiStatus::Ok;
    }
    InputFrame pollInput() noexcept override { return InputFrame{}; }
    void playSound(EffectId) noexcept override {}
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override {
        calls_.emplace_back("loadTiles");
        return loadStatus_;
    }
    std::uint32_t frameCount() const noexcept override { return 0; }
    ApiStatus setInputContext(InputContext) noexcept override {
        calls_.emplace_back("context");
        return ApiStatus::Ok;
    }
    Tilemap map{};

  private:
    std::vector<std::string>& calls_;
    ApiStatus loadStatus_;
};

// Captures what runArcade() prints.
class Log {
  public:
    Log() : file_(std::tmpfile()) {}
    ~Log() { (void)std::fclose(file_); }
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    std::FILE* file() { return file_; }
    std::string text() {
        (void)std::fseek(file_, 0, SEEK_SET);
        std::string out(256, '\0');
        out.resize(std::fread(out.data(), 1, out.size(), file_));
        return out;
    }

  private:
    std::FILE* file_;
};

BackendFactory fakeFactory(std::vector<std::string>& calls, int presents,
                           ApiStatus loadStatus = ApiStatus::Ok) {
    return [&calls, presents, loadStatus](const Options&) {
        BackendStart start;
        calls.emplace_back("start");
        start.api = std::make_unique<RecordingApi>(calls, loadStatus);
        start.stopRequested = [&calls, presents] {
            return std::count(calls.begin(), calls.end(), "present") >= presents;
        };
        return start;
    };
}

template <std::size_t N>
std::optional<Options> parse(const std::array<const char*, N>& args, BackendKind fallback) {
    return parseOptions(static_cast<int>(N), args.data(), fallback);
}

TEST(Options, SR_FW_05_SelectsBackend) {
    const Options none{};
    const std::array<const char*, 2> sdl{"arcade", "--backend=sdl"};
    const std::array<const char*, 2> fpga{"arcade", "--backend=fpga"};
    EXPECT_EQ(parse(sdl, BackendKind::Fpga).value_or(Options{BackendKind::Fpga}).backend,
              BackendKind::Sdl);
    EXPECT_EQ(parse(fpga, BackendKind::Sdl).value_or(none).backend, BackendKind::Fpga);
}

TEST(Options, SR_FW_05_DefaultAppliesWithoutOption) {
    const std::array<const char*, 1> args{"arcade"};
    const Options sentinel{BackendKind::Sdl, true};
    const Options options = parse(args, BackendKind::Fpga).value_or(sentinel);
    EXPECT_EQ(options.backend, BackendKind::Fpga);
    EXPECT_FALSE(options.profile);
}

TEST(Options, ProfileFlag) {
    const std::array<const char*, 2> args{"arcade", "--profile"};
    EXPECT_TRUE(parse(args, BackendKind::Sdl).value_or(Options{}).profile);
}

TEST(Options, RejectsUnknown) {
    const std::array<const char*, 2> bad{"arcade", "--backend=vulkan"};
    const std::array<const char*, 2> other{"arcade", "--nope"};
    EXPECT_FALSE(parse(bad, BackendKind::Sdl).has_value());
    EXPECT_FALSE(parse(other, BackendKind::Sdl).has_value());
}

TEST(Startup, SR_FW_01_LoadsTilesOnceBeforeFirstPresentThenMenu) {
    std::vector<std::string> calls;
    Log log;
    constexpr int kPresents = 70;  // past the splash
    EXPECT_EQ(runArcade(Options{}, fakeFactory(calls, kPresents), log.file()), 0);

    EXPECT_EQ(calls.front(), "start");
    EXPECT_EQ(std::count(calls.begin(), calls.end(), "loadTiles"), 1);
    const auto load = std::find(calls.begin(), calls.end(), "loadTiles");
    const auto present = std::find(calls.begin(), calls.end(), "present");
    const auto menu = std::find(calls.begin(), calls.end(), "context");
    ASSERT_NE(present, calls.end());
    EXPECT_LT(load, present);
    ASSERT_NE(menu, calls.end());  // bootFinished() sets the Menu context (SR-INP-09)
    EXPECT_LT(present, menu);      // the splash came first
}

TEST(Startup, SR_FW_01_AppBootShowsSplashForOneSecondThenMenu) {
    std::vector<std::string> calls;
    Log log;
    EXPECT_EQ(runArcade(Options{}, fakeFactory(calls, TICK_RATE + 1), log.file()), 0);
    const auto menu = std::find(calls.begin(), calls.end(), "context");
    ASSERT_NE(menu, calls.end());
    EXPECT_EQ(std::count(calls.begin(), menu, "present"), TICK_RATE);
}

TEST(Startup, SF_02_FailingBackendPrintsStepAndExitsNonZeroWithin5s) {
    Log log;
    const auto begin = std::chrono::steady_clock::now();
    const int status = runArcade(
        Options{},
        [](const Options&) {
            BackendStart start;
            start.failedStep = "check FPGA manager";
            return start;
        },
        log.file());
    EXPECT_LT(std::chrono::steady_clock::now() - begin, kSf02Limit);
    EXPECT_NE(status, 0);
    EXPECT_EQ(log.text(), "graphics hardware not ready: check FPGA manager\n");
}

TEST(Startup, SF_02_LoadTilesFailureNeverPresents) {
    std::vector<std::string> calls;
    Log log;
    EXPECT_NE(runArcade(Options{}, fakeFactory(calls, 1, ApiStatus::BackendError), log.file()), 0);
    EXPECT_EQ(std::count(calls.begin(), calls.end(), "present"), 0);
    EXPECT_NE(log.text().find(std::string(kStepLoadTiles)), std::string::npos);
}

// The expected picture, from the numbers of section 3.1.1 screen 1, not from BootScreen.
void expectText(Tilemap& map, int col, int row, std::string_view text, TextScale scale) {
    ASSERT_EQ(assets::textToTiles(
                  col, row, text, scale,
                  [](void* m, int c, int r, TileIndex tile) noexcept {
                      (*static_cast<Tilemap*>(
                          m))[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] = tile;
                  },
                  &map),
              ApiStatus::Ok);
}

Tilemap goldenBase() {
    Tilemap golden{};
    expectText(golden, 42, 34, "FPGA ARCADE", TextScale::X4);
    expectText(golden, 47, 44, "SILICON TRIAD - DE1-SOC - 1024X768", TextScale::X1);
    expectText(golden, 58, 53, "STARTING...", TextScale::X1);
    return golden;
}

TEST(BootScreen, Screen1_GoldenTilemapHalfBar) {
    std::vector<std::string> calls;
    RecordingApi api(calls);
    BootScreen boot;
    boot.setProgress(1, 2);
    boot.render(api);
    Tilemap golden = goldenBase();
    for (std::size_t col = 32; col < 64; ++col) {  // half of columns 32-95
        golden[50][col] = assets::kTileMenuGreen;
    }
    EXPECT_EQ(api.map, golden);
}

TEST(BootScreen, Screen1_BarFillsToColumn95) {
    std::vector<std::string> calls;
    RecordingApi api(calls);
    BootScreen boot;
    boot.setProgress(5, 5);
    boot.render(api);
    EXPECT_EQ(boot.barCells(), 64);
    EXPECT_EQ(api.map[50][31], assets::kFontFirstTile);
    EXPECT_EQ(api.map[50][32], assets::kTileMenuGreen);
    EXPECT_EQ(api.map[50][95], assets::kTileMenuGreen);
    EXPECT_EQ(api.map[50][96], assets::kFontFirstTile);
}

TEST(BootScreen, Screen1_NoKeyboardMessageOnRow60) {
    std::vector<std::string> calls;
    RecordingApi api(calls);
    BootScreen boot;
    boot.setKeyboardMissing(true);
    boot.render(api);
    Tilemap golden = goldenBase();
    expectText(golden, 43, 60, "NO KEYBOARD FOUND - PLUG IN A USB KEYBOARD", TextScale::X1);
    EXPECT_EQ(api.map, golden);
}

}  // namespace
}  // namespace arcade
