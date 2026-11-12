#include "app/fpga_start.hpp"

#include <new>

#include "backend_fpga/fpga_backend.hpp"

namespace arcade {

BackendStart startFpgaBackend(const BridgeOpen& bridge, hal::MonotonicClock& clock,
                              InputManager& input, const KeyBindings& bindings) {
    BackendStart start;
    if (bridge.regs == nullptr) {
        start.failedStep = bridge.failedStep;  // SF-02, SR-HAL-09
        return start;
    }
    (void)input.start();  // SR-INP-01; no keyboard is not a failure (SR-INP-16)
    // No sound driver yet: #44 passes it here (SR-HAL-11, SR-HAL-12).
    start.api.reset(new (std::nothrow) FpgaBackend(*bridge.regs, clock, input, bindings));
    if (start.api == nullptr) {
        start.failedStep = kStepStartBackend;
        return start;
    }
    start.keyboardPresent = [&input] { return input.hasKeyboard(); };  // SR-INP-16
    // finalFrameTicks stays 0: on the board the last frame stays until power-off (SR-HAL-07).
    return start;
}

}  // namespace arcade
