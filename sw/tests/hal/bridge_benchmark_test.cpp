#include "hal/bridge_benchmark.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {
using namespace arcade::hal;
using namespace arcade::hal::bench;

/// Parses `args`; on success copies the result into `out`.
bool parse(const std::vector<std::string>& args, Options& out, std::string& error) {
    const auto r = parseOptions(args, error);
    if (!r) {
        return false;
    }
    out = *r;
    return true;
}

}  // namespace

// §3.1.4 data rate: 12,288 bytes x 60 ticks = 737,280 bytes/s.
TEST(BridgeBenchmark, DataRateTargetIs12288Times60) { EXPECT_EQ(kTargetBytesPerSecond, 737280u); }

TEST(BridgeBenchmark, VerdictAtAndAroundTarget) {
    EXPECT_TRUE(meetsDataRate(737280u));
    EXPECT_TRUE(meetsDataRate(5000000u));
    EXPECT_FALSE(meetsDataRate(737279u));
    EXPECT_FALSE(meetsDataRate(0u));
}

TEST(BridgeBenchmark, BytesPerSecond) {
    EXPECT_EQ(bytesPerSecond(12288u, 1000000000u), 12288u);  // one bank in 1 s
    EXPECT_EQ(bytesPerSecond(std::uint64_t{12288} * 600u, 10000000000ull), 737280u);
    EXPECT_EQ(bytesPerSecond(1000u, 0u), 0u);  // no division by zero
}

// SR-HAL-06 / §3.1.4: Front Bank is bit 0 of 0x804C; the benchmark writes the other bank.
TEST(BridgeBenchmark, BackBankIsTheOtherBank) {
    EXPECT_EQ(backBankOffset(0u), kOffsetTilemapBank1);
    EXPECT_EQ(backBankOffset(1u), kOffsetTilemapBank0);
    EXPECT_EQ(backBankOffset(0xFFFFFFFEu), kOffsetTilemapBank1);  // only bit 0 counts
}

TEST(BridgeBenchmark, WriteBank8FillsExactlyOneBank) {
    std::array<std::uint8_t, TILEMAP_BANK_SIZE + 4> mem{};
    mem.fill(0xEE);
    writeBank8(mem.data(), 0x5A);
    for (std::size_t i = 0; i < TILEMAP_BANK_SIZE; ++i) {
        ASSERT_EQ(mem[i], 0x5A) << i;
    }
    for (std::size_t i = TILEMAP_BANK_SIZE; i < mem.size(); ++i) {
        EXPECT_EQ(mem[i], 0xEE) << "wrote past the bank at " << i;
    }
}

TEST(BridgeBenchmark, WriteBank32FillsExactlyOneBank) {
    constexpr std::size_t kWords = TILEMAP_BANK_SIZE / 4;
    std::array<std::uint32_t, kWords + 1> mem{};
    mem.fill(0xEEEEEEEEu);
    writeBank32(mem.data(), 0x5A);
    for (std::size_t i = 0; i < kWords; ++i) {
        ASSERT_EQ(mem[i], 0x5A5A5A5Au) << i;
    }
    EXPECT_EQ(mem[kWords], 0xEEEEEEEEu) << "wrote past the bank";
}

TEST(BridgeBenchmark, DefaultOptions) {
    std::string err;
    Options o;
    ASSERT_TRUE(parse({}, o, err)) << err;
    EXPECT_EQ(o.passes, 600u);
    EXPECT_EQ(o.value, 0u);
    EXPECT_EQ(o.widths.size(), 2u);
    EXPECT_FALSE(o.help);
}

TEST(BridgeBenchmark, ParsesAllOptions) {
    std::string err;
    Options o;
    ASSERT_TRUE(parse({"--passes", "10", "--width", "32", "--value", "0x1F"}, o, err)) << err;
    EXPECT_EQ(o.passes, 10u);
    ASSERT_EQ(o.widths.size(), 1u);
    EXPECT_EQ(o.widths[0], WriteWidth::k32);
    EXPECT_EQ(o.value, 0x1Fu);
    Options h;
    ASSERT_TRUE(parse({"--help"}, h, err)) << err;
    EXPECT_TRUE(h.help);
}

TEST(BridgeBenchmark, RejectsBadOptions) {
    const std::vector<std::vector<std::string>> bad = {
        {"--passes"},       {"--passes", "0"},   {"--passes", "100001"},
        {"--passes", "-5"}, {"--passes", "12x"}, {"--width", "16"},
        {"--value", "256"}, {"--value", ""},     {"--speed", "1"},
    };
    for (const auto& args : bad) {
        std::string err;
        Options o;
        EXPECT_FALSE(parse(args, o, err)) << args[0];
        EXPECT_FALSE(err.empty());
    }
}
