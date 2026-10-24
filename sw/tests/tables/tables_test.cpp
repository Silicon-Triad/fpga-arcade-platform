// Checks of the frozen software table definitions (issue #6): compile-time shape of the rows and
// constants, and the TBL-SoundEffect data against srs-huzaifa-rev3.md section 5.
#include "arcade/tables.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace arcade {
namespace {

// Header-only checks: field types match Chapter 5 (R-4.1.8).
static_assert(SCORE_MAX == 999'999, "SR-UI-04: SCORE_MAX");
static_assert(HISCORE_SLOTS == 10 && kRankMin == 1 && kRankMax == 10, "SR-FW-10: rank 1-10");
static_assert(INITIALS_LEN == 3, "SR-UI-10: three initials");
static_assert(HISCORE_FILE == "/var/lib/arcade/highscores.csv", "SR-FW-08: HISCORE_FILE");
static_assert(SNAKE == 0 && BREAKOUT == 1, "TBL-GameRegistry release-1.0 rows");
static_assert(TONE_FREQ_MIN == 100 && TONE_FREQ_MAX == 4'000, "SR-AUD-04");
static_assert(TONE_DURATION_MIN == 10 && TONE_DURATION_MAX == 2'000, "SR-AUD-05");

static_assert(std::is_same_v<decltype(GameRegistryRow::game_id), std::uint8_t>, "UINT8");
static_assert(std::is_same_v<decltype(GameRegistryRow::menu_order), std::uint8_t>, "UINT8");
static_assert(std::is_same_v<GameFactory, std::unique_ptr<Game> (*)()>, "SR-FW-03 factory");
static_assert(std::is_same_v<decltype(HighScoreRow::game_id), std::uint8_t>, "UINT8");
static_assert(std::is_same_v<decltype(HighScoreRow::rank), std::uint8_t>, "UINT8");
static_assert(sizeof(HighScoreRow::initials) == 3, "CHAR(3)");
static_assert(std::is_same_v<decltype(HighScoreRow::score), std::uint32_t>, "UINT32");
static_assert(std::is_same_v<decltype(HighScoreRow::seq), std::uint32_t>, "UINT32");
static_assert(static_cast<int>(HighScoreColumn::GameId) == 0 &&
                  static_cast<int>(HighScoreColumn::Rank) == 1 &&
                  static_cast<int>(HighScoreColumn::Initials) == 2 &&
                  static_cast<int>(HighScoreColumn::Score) == 3 &&
                  static_cast<int>(HighScoreColumn::Seq) == 4 && kHighScoreColumnCount == 5,
              "TBL-HighScore: game_id,rank,initials,score,seq");
static_assert(std::is_same_v<decltype(KeyBindingRow::context), InputContext>, "ENUM");
static_assert(std::is_same_v<decltype(KeyBindingRow::key_code), std::uint16_t>, "UINT16");
static_assert(std::is_same_v<decltype(KeyBindingRow::action), Action>, "ENUM");
static_assert(std::is_same_v<std::underlying_type_t<EffectId>, std::uint8_t>, "UINT8");
static_assert(std::is_same_v<decltype(SoundNote::note_no), std::uint8_t>, "UINT8");
static_assert(std::is_same_v<decltype(SoundNote::freq_hz), std::uint16_t>, "UINT16");
static_assert(std::is_same_v<decltype(SoundNote::dur_ms), std::uint16_t>, "UINT16");

static_assert(TICK_RATE == 60, "SR-FW-02: TICK_RATE");
static_assert(msToTicks(33) == 2 && msToTicks(50) == 3 && msToTicks(83) == 5 &&
                  msToTicks(100) == 6 && msToTicks(117) == 7 && msToTicks(133) == 8 &&
                  msToTicks(150) == 9,
              "SR-HAL-11: SRS section 5 tick counts");
static_assert(msToTicks(TONE_DURATION_MAX) == 120, "SR-AUD-05: 2,000 ms fits in uint8_t");

/// Game ticks per second, used to check msToTicks() independently (TICK_RATE, SR-FW-02).
constexpr int kTickRate = 60;

struct ExpectedNote {
    EffectId effect;
    std::uint8_t note_no;
    std::uint16_t hz;
    std::uint16_t ms;
    int ticks;
};

// srs-huzaifa-rev3.md section 5 contents table, retyped independently of tables.hpp.
constexpr std::array<ExpectedNote, 14> kExpected{{
    {EffectId::Eat, 1, 784, 33, 2},
    {EffectId::Eat, 2, 1047, 50, 3},
    {EffectId::BrickHit, 1, 1319, 33, 2},
    {EffectId::PaddleHit, 1, 523, 33, 2},
    {EffectId::LifeLost, 1, 392, 100, 6},
    {EffectId::LifeLost, 2, 330, 100, 6},
    {EffectId::LifeLost, 3, 262, 150, 9},
    {EffectId::GameOver, 1, 523, 117, 7},
    {EffectId::GameOver, 2, 392, 117, 7},
    {EffectId::GameOver, 3, 330, 117, 7},
    {EffectId::GameOver, 4, 262, 133, 8},
    {EffectId::MenuMove, 1, 880, 33, 2},
    {EffectId::MenuSelect, 1, 659, 50, 3},
    {EffectId::MenuSelect, 2, 1047, 83, 5},
}};

/// Duration of `ticks` game ticks in ms, rounded to the nearest ms as the SRS table does.
constexpr int ticksToMs(int ticks) { return (ticks * 1000 + kTickRate / 2) / kTickRate; }

TEST(Tables, TBL_SoundEffect_MatchesSrsSection5) {
    ASSERT_EQ(kSoundEffectTable.size(), kExpected.size());
    for (std::size_t i = 0; i < kExpected.size(); ++i) {
        const SoundNote& n = kSoundEffectTable[i];
        const ExpectedNote& e = kExpected[i];
        EXPECT_EQ(n.effect_id, e.effect) << "row " << i;
        EXPECT_EQ(n.note_no, e.note_no) << "row " << i;
        EXPECT_EQ(n.freq_hz, e.hz) << "row " << i;
        EXPECT_EQ(n.dur_ms, e.ms) << "row " << i;
        EXPECT_EQ(ticksToMs(e.ticks), n.dur_ms) << "row " << i;
        EXPECT_EQ(msToTicks(n.dur_ms), e.ticks) << "row " << i;
    }
}

TEST(Tables, SR_AUD_04_SR_AUD_05_NotesWithinToneLimitsAndWholeTicks) {
    for (const SoundNote& n : kSoundEffectTable) {
        EXPECT_GE(n.freq_hz, TONE_FREQ_MIN);
        EXPECT_LE(n.freq_hz, TONE_FREQ_MAX);
        EXPECT_GE(n.dur_ms, TONE_DURATION_MIN);
        EXPECT_LE(n.dur_ms, TONE_DURATION_MAX);
        const int ticks = (n.dur_ms * kTickRate + 500) / 1000;
        EXPECT_GE(ticks, 1);
        EXPECT_EQ(ticksToMs(ticks), n.dur_ms) << "not a whole number of ticks: " << n.dur_ms;
    }
}

TEST(Tables, TBL_SoundEffect_KeysOrderedAndEveryEffectAtMost500ms) {
    std::array<int, kEffectCount> total{};
    std::array<int, kEffectCount> notes{};
    for (const SoundNote& n : kSoundEffectTable) {
        const auto id = static_cast<std::size_t>(n.effect_id);
        ASSERT_LT(id, kEffectCount);
        ++notes[id];
        EXPECT_EQ(n.note_no, notes[id]) << "note_no out of play order";
        EXPECT_GE(n.note_no, kNoteNoMin);
        EXPECT_LE(n.note_no, kNoteNoMax);
        total[id] += n.dur_ms;
    }
    for (std::size_t id = 0; id < kEffectCount; ++id) {
        EXPECT_GE(notes[id], 1) << "effect " << id << " has no notes";
        EXPECT_LE(total[id], kEffectTotalMaxMs) << "effect " << id;
    }
    EXPECT_EQ(total[static_cast<std::size_t>(EffectId::GameOver)], 484);
}

TEST(Tables, TBL_SoundEffect_NamesMatchEffectId) {
    constexpr std::array<std::string_view, kEffectCount> kNames{
        "Eat", "BrickHit", "PaddleHit", "LifeLost", "GameOver", "MenuMove", "MenuSelect"};
    for (const SoundNote& n : kSoundEffectTable) {
        EXPECT_EQ(n.name, kNames[static_cast<std::size_t>(n.effect_id)]);
        EXPECT_LE(n.name.size(), kEffectNameMax);
    }
}

}  // namespace
}  // namespace arcade
