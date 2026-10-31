// Fake backend for engine tests: keeps the tilemap and records sounds and input contexts.
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "arcade/engine_api.hpp"

namespace arcade {

class FakeEngineApi final : public EngineApi {
  public:
    ApiStatus setTile(int col, int row, TileIndex tile) noexcept override {
        cells[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = tile;
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
    ApiStatus drawText(int, int row, std::string_view text, TextScale) noexcept override {
        texts.emplace_back(std::to_string(row) + ":" + std::string(text));
        return ApiStatus::Ok;
    }
    void clear(TileIndex tile) noexcept override {
        for (auto& line : cells) {
            line.fill(tile);
        }
        texts.clear();
    }
    ApiStatus present() noexcept override {
        ++presents;
        return ApiStatus::Ok;
    }
    InputFrame pollInput() noexcept override {
        ++polls;
        const InputFrame frame = nextInput;
        nextInput = InputFrame{};
        return frame;
    }
    void playSound(EffectId) noexcept override { ++sounds; }
    void setMute(bool) noexcept override {}
    ApiStatus loadTiles(const TileSet&, const Palette&) noexcept override { return ApiStatus::Ok; }
    std::uint32_t frameCount() const noexcept override { return 0; }
    ApiStatus setInputContext(InputContext context) noexcept override {
        contexts.push_back(context);
        return ApiStatus::Ok;
    }

    std::array<std::array<TileIndex, kGridCols>, kGridRows> cells{};
    std::vector<std::string> texts;
    std::vector<InputContext> contexts;
    int sounds = 0;
    InputFrame nextInput;  ///< Returned once by the next pollInput().
    int polls = 0;
    int presents = 0;
};

}  // namespace arcade
