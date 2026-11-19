#include "app/fpga_start.hpp"

#include <memory>
#include <new>
#include <utility>

#include "backend_fpga/fpga_backend.hpp"
#include "hal/tone_sequencer.hpp"

namespace arcade {
namespace {

/// The FpgaBackend sound hook on the HAL sound sequencer (SR-HAL-11, SR-HAL-12). A thin
/// adapter: the HAL may not depend on sw/backend_fpga/ (QA-01), so ToneSequencer does not
/// implement FpgaSound itself.
class ToneSound final : public FpgaSound {
  public:
    explicit ToneSound(hal::RegisterSpace& regs) noexcept : seq_(regs) {}
    void play(EffectId effectId) noexcept override { seq_.play(effectId); }
    void setMute(bool on) noexcept override { seq_.setMute(on); }
    void tick() noexcept override { seq_.tick(); }

  private:
    hal::ToneSequencer seq_;
};

}  // namespace

BackendStart startFpgaBackend(const BridgeOpen& bridge, hal::MonotonicClock& clock,
                              InputManager& input, const KeyBindings& bindings) {
    BackendStart start;
    if (bridge.regs == nullptr) {
        start.failedStep = bridge.failedStep;  // SF-02, SR-HAL-09
        return start;
    }
    (void)input.start();  // SR-INP-01; no keyboard is not a failure (SR-INP-16)
    // SR-HAL-11, SR-HAL-12: the sound driver, kept alive in `start` after the backend. If it
    // cannot be allocated the backend runs silent (SR-FW-07). No bridge access.
    std::shared_ptr<ToneSound> sound(new (std::nothrow) ToneSound(*bridge.regs));
    start.api.reset(new (std::nothrow)
                        FpgaBackend(*bridge.regs, clock, input, bindings, sound.get()));
    start.keepAlive = std::move(sound);
    if (start.api == nullptr) {
        start.failedStep = kStepStartBackend;
        return start;
    }
    start.keyboardPresent = [&input] { return input.hasKeyboard(); };  // SR-INP-16
    // finalFrameTicks stays 0: on the board the last frame stays until power-off (SR-HAL-07).
    return start;
}

}  // namespace arcade
