#pragma once

#include <cstddef>
#include <cstdint>

#include "arcade/types.hpp"
#include "hal/register_map.hpp"
#include "hal/register_space.hpp"

namespace arcade::hal {

/// The two defined bits of the Audio Control Register (§3.1.4), from register_map.hpp. Only
/// these can be passed to setAudioControlBit(), so no other bit (reserved, or >= 32) is
/// ever written.
enum class AudioControlBit : std::uint8_t {
    kEnable = kBitAudioEnable,  ///< Bit 0, audio enable: set by #43 after codec set-up (SR-HAL-10).
    kMute = kBitAudioMute,      ///< Bit 1, mute: written by setMute() (SR-HAL-12).
};

/// Sets or clears one bit of the Audio Control Register by read-modify-write (§3.1.4: R/W),
/// leaving every other bit as read. The single access path to Audio Control: setMute()
/// (SR-HAL-12) uses kMute, and #43 sets kEnable (SR-HAL-10) with it.
/// One read32, then one write32. No call throws (R-4.1.5).
void setAudioControlBit(RegisterSpace& regs, AudioControlBit bit, bool on) noexcept;

/// Sound sequencer of the HAL (SR-HAL-11, SR-HAL-12, TBL-SoundEffect). Plays the notes of one
/// TBL-SoundEffect effect through the Tone Frequency and Tone Duration registers, one note per
/// pair of writes, and writes the mute bit. Notes come from kSoundEffectTable (tables.hpp),
/// never copied. One voice (CON-13): a new play() replaces the effect still playing.
///
/// Timing: play() is called during a tick's game update, before that tick's present(), which
/// calls tick() once. A note of n ticks (msToTicks(dur_ms)) started in tick T ends in tick
/// T + n, so the next note is written by the tick() call of tick T + n. Fixed-size state, no
/// heap use, no blocking, no exceptions (R-4.1.5, R-4.1.6).
class ToneSequencer {
  public:
    /// Bind to a register space that must outlive this object. No bridge access.
    explicit ToneSequencer(RegisterSpace& regs) noexcept;

    /// SR-HAL-11: look up `effectId` in TBL-SoundEffect and write its first note at once
    /// (frequency, then duration, which starts the tone). Replaces any effect still playing.
    /// An effect id with no rows is ignored: no write, and the current effect plays on.
    void play(EffectId effectId) noexcept;
    /// SR-HAL-12: write Audio Control bit 1 (mute) to `on`; bit 0 (audio enable, #43) and the
    /// other bits are left as read. Sequencing goes on while muted (SR-AUD-08 silences it).
    void setMute(bool on) noexcept;
    /// SR-HAL-11: once per game tick. Writes the next note in the tick the previous one ends;
    /// writes nothing in other ticks or after the last note.
    void tick() noexcept;

    /// True while an effect has a note sounding or still to write. For tests only; the driver
    /// itself never needs it.
    bool playing() const noexcept { return ticksLeft_ > 0; }

  private:
    void writeNote(std::size_t row) noexcept;

    RegisterSpace& regs_;
    std::size_t nextRow_ = 0;  ///< Next kSoundEffectTable row to write.
    std::size_t endRow_ = 0;   ///< One past the current effect's last row.
    unsigned ticksLeft_ = 0;   ///< Ticks until the sounding note ends; 0 when idle.
    bool skipTick_ = false;    ///< True after play(): its own tick's present() is not counted.
};

}  // namespace arcade::hal
