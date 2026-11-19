// Unit tests for the HAL sound sequencer and mute (issue #44, SR-HAL-11, SR-HAL-12,
// TBL-SoundEffect), on the mock register space.
#include "hal/tone_sequencer.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "arcade/tables.hpp"
#include "hal/register_map.hpp"
#include "mock_register_space.hpp"

namespace arcade::hal {
namespace {

using test::Access;
using test::MockRegisterSpace;

/// The register writes made in one game tick.
using TickWrites = std::vector<Access>;

/// Runs one game tick as the engine does: `before` (the game update, e.g. play()), then the
/// tick() that present() makes. Returns the accesses made in this tick.
template <typename F>
TickWrites runTick(MockRegisterSpace& regs, ToneSequencer& seq, F before) {
    regs.log.clear();
    before();
    seq.tick();
    return regs.log;
}
TickWrites runTick(MockRegisterSpace& regs, ToneSequencer& seq) {
    return runTick(regs, seq, [] {});
}

/// The rows of `id` in TBL-SoundEffect, in play order.
std::vector<SoundNote> notesOf(EffectId id) {
    std::vector<SoundNote> notes;
    for (const SoundNote& note : kSoundEffectTable) {
        if (note.effect_id == id) {
            notes.push_back(note);
        }
    }
    return notes;
}

void expectNote(const TickWrites& w, const SoundNote& note, int tick) {
    ASSERT_EQ(w.size(), 2U) << "tick " << tick;
    EXPECT_EQ(w[0].kind, Access::Kind::kWrite32);
    EXPECT_EQ(w[0].offset, kOffsetToneFrequency) << "frequency first, tick " << tick;
    EXPECT_EQ(w[0].value, note.freq_hz);
    EXPECT_EQ(w[1].kind, Access::Kind::kWrite32);
    EXPECT_EQ(w[1].offset, kOffsetToneDuration) << "then duration, tick " << tick;
    EXPECT_EQ(w[1].value, note.dur_ms);
}

TEST(ToneSequencer, SR_HAL_11_EveryEffectWritesEachNoteInTheTickThePreviousEnds) {
    for (std::size_t e = 0; e < kEffectCount; ++e) {
        const auto id = static_cast<EffectId>(e);
        const std::vector<SoundNote> notes = notesOf(id);
        ASSERT_FALSE(notes.empty()) << "effect " << e;
        MockRegisterSpace regs;
        ToneSequencer seq(regs);

        // Expected start tick of each note: the sum of the earlier notes' tick counts.
        std::vector<int> startTick;
        int t = 0;
        for (const SoundNote& note : notes) {
            startTick.push_back(t);
            t += msToTicks(note.dur_ms);
        }
        const int endTick = t;

        std::size_t next = 0;
        for (int tick = 0; tick <= endTick + 10; ++tick) {
            const TickWrites w =
                tick == 0 ? runTick(regs, seq, [&] { seq.play(id); }) : runTick(regs, seq);
            if (next < notes.size() && tick == startTick[next]) {
                expectNote(w, notes[next], tick);
                ++next;
            } else {
                EXPECT_TRUE(w.empty()) << "effect " << e << " tick " << tick;
            }
        }
        EXPECT_EQ(next, notes.size()) << "effect " << e;
        EXPECT_FALSE(seq.playing());
    }
}

TEST(ToneSequencer, SR_HAL_11_SingleNoteEffectWritesOnceThenNothing) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const std::vector<SoundNote> notes = notesOf(EffectId::BrickHit);
    ASSERT_EQ(notes.size(), 1U);
    expectNote(runTick(regs, seq, [&] { seq.play(EffectId::BrickHit); }), notes[0], 0);
    for (int tick = 1; tick < 30; ++tick) {
        EXPECT_TRUE(runTick(regs, seq).empty()) << tick;
    }
}

TEST(ToneSequencer, SR_HAL_11_NewPlayReplacesTheEffectAtOnce) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const std::vector<SoundNote> over = notesOf(EffectId::GameOver);
    const std::vector<SoundNote> eat = notesOf(EffectId::Eat);
    ASSERT_GE(over.size(), 2U);
    ASSERT_EQ(eat.size(), 2U);
    runTick(regs, seq, [&] { seq.play(EffectId::GameOver); });
    // Mid-note: GameOver's first note has more ticks left; Eat starts in this same tick.
    expectNote(runTick(regs, seq, [&] { seq.play(EffectId::Eat); }), eat[0], 1);
    std::vector<Access> all;
    for (int tick = 2; tick < 60; ++tick) {
        const TickWrites w = runTick(regs, seq);
        all.insert(all.end(), w.begin(), w.end());
    }
    ASSERT_EQ(all.size(), 2U);  // only Eat's second note; no GameOver note ever again
    expectNote(all, eat[1], -1);
}

TEST(ToneSequencer, SR_HAL_11_ReplacingTheSameEffectRestartsItsFirstNote) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const std::vector<SoundNote> life = notesOf(EffectId::LifeLost);
    const int first = msToTicks(life[0].dur_ms);
    runTick(regs, seq, [&] { seq.play(EffectId::LifeLost); });
    for (int tick = 1; tick < first; ++tick) {
        runTick(regs, seq);
    }
    // In the tick note 2 would start, a new play() writes note 1 only.
    expectNote(runTick(regs, seq, [&] { seq.play(EffectId::LifeLost); }), life[0], first);
}

TEST(ToneSequencer, SR_HAL_11_UnknownEffectWritesNothing) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const TickWrites w = runTick(regs, seq, [&] { seq.play(static_cast<EffectId>(kEffectCount)); });
    EXPECT_TRUE(w.empty());
    EXPECT_TRUE(runTick(regs, seq, [&] { seq.play(static_cast<EffectId>(0xFF)); }).empty());
    EXPECT_FALSE(seq.playing());
}

TEST(ToneSequencer, SR_HAL_11_UnknownEffectLeavesTheCurrentEffectPlaying) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const std::vector<SoundNote> eat = notesOf(EffectId::Eat);
    runTick(regs, seq, [&] { seq.play(EffectId::Eat); });
    EXPECT_TRUE(runTick(regs, seq, [&] { seq.play(static_cast<EffectId>(kEffectCount)); }).empty());
    expectNote(runTick(regs, seq), eat[1], msToTicks(eat[0].dur_ms));
}

TEST(ToneSequencer, SR_HAL_12_SetMuteWritesOnlyBit1) {
    for (const bool enable : {false, true}) {
        for (const bool on : {true, false}) {
            MockRegisterSpace regs;
            const std::uint32_t enableBit = enable ? (1U << kBitAudioEnable) : 0U;
            const std::uint32_t muteBit = 1U << kBitAudioMute;
            regs.audioControl = enableBit | (on ? 0U : muteBit);  // start from the other state
            ToneSequencer seq(regs);
            seq.setMute(on);
            ASSERT_EQ(regs.log.size(), 2U);
            EXPECT_EQ(regs.log[0].kind, Access::Kind::kRead32);
            EXPECT_EQ(regs.log[0].offset, kOffsetAudioControl);
            EXPECT_EQ(regs.log[1].kind, Access::Kind::kWrite32);
            EXPECT_EQ(regs.log[1].offset, kOffsetAudioControl);
            EXPECT_EQ(regs.log[1].value, enableBit | (on ? muteBit : 0U))
                << "enable " << enable << " mute " << on;
        }
    }
}

TEST(ToneSequencer, SR_HAL_12_SetMuteIsIdempotentAndKeepsTheEnableBit) {
    MockRegisterSpace regs;
    regs.audioControl = 1U << kBitAudioEnable;
    ToneSequencer seq(regs);
    seq.setMute(true);
    seq.setMute(true);
    EXPECT_EQ(regs.audioControl, (1U << kBitAudioEnable) | (1U << kBitAudioMute));
    seq.setMute(false);
    EXPECT_EQ(regs.audioControl, 1U << kBitAudioEnable);
}

TEST(ToneSequencer, SR_HAL_10_SetAudioControlBitSetsEnableWithoutTouchingMute) {
    // The path #43 uses to set the audio-enable bit (SR-HAL-10).
    MockRegisterSpace regs;
    regs.audioControl = 1U << kBitAudioMute;
    setAudioControlBit(regs, kBitAudioEnable, true);
    EXPECT_EQ(regs.audioControl, (1U << kBitAudioEnable) | (1U << kBitAudioMute));
}

TEST(ToneSequencer, SR_HAL_11_SR_HAL_12_SequencingContinuesWhileMuted) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    const std::vector<SoundNote> sel = notesOf(EffectId::MenuSelect);
    ASSERT_EQ(sel.size(), 2U);
    runTick(regs, seq, [&] { seq.play(EffectId::MenuSelect); });
    seq.setMute(true);
    for (int tick = 1; tick < msToTicks(sel[0].dur_ms); ++tick) {
        EXPECT_TRUE(runTick(regs, seq).empty());
    }
    expectNote(runTick(regs, seq), sel[1], msToTicks(sel[0].dur_ms));
    EXPECT_NE(regs.audioControl & (1U << kBitAudioMute), 0U);
}

TEST(ToneSequencer, SR_HAL_11_TickWithoutAnEffectWritesNothing) {
    MockRegisterSpace regs;
    ToneSequencer seq(regs);
    for (int tick = 0; tick < 10; ++tick) {
        EXPECT_TRUE(runTick(regs, seq).empty());
    }
}

}  // namespace
}  // namespace arcade::hal
