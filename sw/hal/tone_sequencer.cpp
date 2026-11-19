#include "hal/tone_sequencer.hpp"

#include "arcade/tables.hpp"
#include "hal/register_map.hpp"

namespace arcade::hal {
namespace {

/// TBL-SoundEffect rows must fit the Tone registers' ranges (SR-AUD-04, SR-AUD-05) and last at
/// least one tick, or the sequencer could never reach the next note (SR-HAL-11).
constexpr bool tableFitsTheRegisters() noexcept {
    for (const SoundNote& note : kSoundEffectTable) {
        if (note.freq_hz < TONE_FREQ_MIN || note.freq_hz > TONE_FREQ_MAX ||
            note.dur_ms < TONE_DURATION_MIN || note.dur_ms > TONE_DURATION_MAX ||
            msToTicks(note.dur_ms) == 0) {
            return false;
        }
    }
    return true;
}
static_assert(tableFitsTheRegisters(), "TBL-SoundEffect: SR-AUD-04, SR-AUD-05, SR-HAL-11");

}  // namespace

void setAudioControlBit(RegisterSpace& regs, unsigned bit, bool on) noexcept {
    // §3.1.4: Audio Control is R/W, so the hardware register is the only copy of bit 0 (#43)
    // and bit 1 (#44); no shadow to keep in step.
    const std::uint32_t mask = 1U << bit;
    const std::uint32_t value = regs.read32(kOffsetAudioControl);
    regs.write32(kOffsetAudioControl, on ? (value | mask) : (value & ~mask));
}

ToneSequencer::ToneSequencer(RegisterSpace& regs) noexcept : regs_(regs) {}

void ToneSequencer::play(EffectId effectId) noexcept {
    // SR-HAL-11: rows are ordered by effect_id then note_no (tables.hpp), so one effect's notes
    // are consecutive, in play order.
    std::size_t first = kSoundEffectTable.size();
    std::size_t end = first;
    for (std::size_t row = 0; row < kSoundEffectTable.size(); ++row) {
        if (kSoundEffectTable[row].effect_id == effectId) {
            if (first == kSoundEffectTable.size()) {
                first = row;
            }
            end = row + 1;
        }
    }
    if (first == kSoundEffectTable.size()) {
        return;  // unknown effect: no write, the current effect plays on
    }
    endRow_ = end;  // SR-HAL-11: replaces any effect still playing
    writeNote(first);
    skipTick_ = true;
}

void ToneSequencer::setMute(bool on) noexcept {
    setAudioControlBit(regs_, kBitAudioMute, on);  // SR-HAL-12
}

void ToneSequencer::tick() noexcept {
    if (skipTick_) {
        skipTick_ = false;  // the tick play() was called in: its note started this tick
        return;
    }
    if (ticksLeft_ == 0) {
        return;  // idle
    }
    --ticksLeft_;
    if (ticksLeft_ == 0 && nextRow_ < endRow_) {
        writeNote(nextRow_);  // SR-HAL-11: in the tick the previous note ends
    }
}

void ToneSequencer::writeNote(std::size_t row) noexcept {
    const SoundNote& note = kSoundEffectTable[row];
    // Frequency first: the Tone Duration write starts the tone with the current frequency
    // (hw/rtl/bridge_regs.sv A6, tone_start).
    regs_.write32(kOffsetToneFrequency, note.freq_hz);
    regs_.write32(kOffsetToneDuration, note.dur_ms);
    ticksLeft_ = msToTicks(note.dur_ms);
    nextRow_ = row + 1;
}

}  // namespace arcade::hal
