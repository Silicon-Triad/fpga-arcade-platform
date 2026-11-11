#include "hal/bridge_benchmark.hpp"

#include <cstddef>
#include <cstdlib>

namespace arcade::hal::bench {
namespace {

constexpr std::uint32_t kMaxPasses = 100000;           // tool limit, not an SRS value
constexpr unsigned long kMaxTileValue = 255;           // one byte per tilemap cell (TBL-Tilemap)
constexpr std::uint32_t kByteReplicate = 0x01010101u;  // copies one byte into all 4 lanes

/// Parses a whole decimal or 0x-hex number; std::nullopt if anything else is in `text`.
std::optional<unsigned long> parseNumber(const std::string& text) {
    if (text.empty() || text[0] == '-' || text[0] == '+') {
        return std::nullopt;
    }
    char* end = nullptr;
    const unsigned long n = std::strtoul(text.c_str(), &end, 0);
    if (end == nullptr || *end != '\0') {
        return std::nullopt;
    }
    return n;
}

}  // namespace

std::string usage() {
    return "usage: bridge_benchmark [--passes N] [--width 8|32|both] [--value N]\n"
           "  Writes one full tilemap bank (the back bank) N times over the lightweight\n"
           "  HPS-to-FPGA bridge and reports bytes/s against 737,280 B/s (SRS 3.1.4).\n"
           "  Run as root, only after arcade_top is loaded and the ID register reads\n"
           "  0xA2CADE01. Defaults: --passes 600 --width both --value 0.\n";
}

std::optional<Options> parseOptions(const std::vector<std::string>& args, std::string& error) {
    Options out;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--help" || a == "-h") {
            out.help = true;
            continue;
        }
        if (a != "--passes" && a != "--width" && a != "--value") {
            error = "unknown argument: " + a;
            return std::nullopt;
        }
        if (i + 1 >= args.size()) {
            error = a + " needs a value";
            return std::nullopt;
        }
        const std::string& v = args[++i];
        if (a == "--width") {
            if (v == "8") {
                out.widths = {WriteWidth::k8};
            } else if (v == "32") {
                out.widths = {WriteWidth::k32};
            } else if (v == "both") {
                out.widths = {WriteWidth::k8, WriteWidth::k32};
            } else {
                error = "--width must be 8, 32 or both";
                return std::nullopt;
            }
            continue;
        }
        const auto n = parseNumber(v);
        if (a == "--passes") {
            if (!n || *n < 1 || *n > kMaxPasses) {
                error = "--passes must be 1 to 100000";
                return std::nullopt;
            }
            out.passes = static_cast<std::uint32_t>(*n);
        } else {
            if (!n || *n > kMaxTileValue) {
                error = "--value must be 0 to 255";
                return std::nullopt;
            }
            out.value = static_cast<std::uint8_t>(*n);
        }
    }
    return out;
}

std::uint32_t backBankOffset(std::uint32_t swapReqFrontBank) noexcept {
    // §3.1.4: bit 0 of the read side is the Front Bank; the HAL writes the other one.
    const bool frontIsBank1 = ((swapReqFrontBank >> kBitFrontBank) & 1u) != 0;
    return frontIsBank1 ? kOffsetTilemapBank0 : kOffsetTilemapBank1;
}

void writeBank8(volatile std::uint8_t* bank, std::uint8_t value) noexcept {
    for (std::size_t i = 0; i < TILEMAP_BANK_SIZE; ++i) {
        bank[i] = value;
    }
}

void writeBank32(volatile std::uint32_t* bank, std::uint8_t value) noexcept {
    const std::uint32_t word = kByteReplicate * value;
    for (std::size_t i = 0; i < TILEMAP_BANK_SIZE / sizeof(std::uint32_t); ++i) {
        bank[i] = word;
    }
}

std::uint64_t bytesPerSecond(std::uint64_t bytes, std::uint64_t nanoseconds) noexcept {
    constexpr std::uint64_t kNsPerSecond = 1000000000ull;
    if (nanoseconds == 0) {
        return 0;
    }
    // bytes stays far below 2^64 / 1e9 for any allowed --passes, so this cannot overflow.
    return bytes * kNsPerSecond / nanoseconds;
}

bool meetsDataRate(std::uint64_t measuredBytesPerSecond) noexcept {
    return measuredBytesPerSecond >= kTargetBytesPerSecond;  // §3.1.4 data rate
}

}  // namespace arcade::hal::bench
