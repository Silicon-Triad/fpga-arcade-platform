/// @file
/// Start-up of the FPGA backend (SR-FW-01, SR-FW-05, SF-02).
#pragma once

#include <string_view>

#include "app/startup.hpp"
#include "hal/register_space.hpp"
#include "hal/tilemap_hal.hpp"
#include "input/input_manager.hpp"
#include "input/key_bindings.hpp"

namespace arcade {

/// The bridge window as #34 hands it over: mapped and checked (SR-HAL-01, SR-HAL-02,
/// SR-HAL-09), or the step that failed. Until #34 lands, main.cpp always gives the failure.
struct BridgeOpen {
    /// The mapped window, or nullptr if the hardware is not ready. Must outlive the backend.
    hal::RegisterSpace* regs = nullptr;
    /// SF-02: the failed step, printed when `regs` is nullptr.
    std::string_view failedStep = kStepStartBackend;
};

/// Starts the FPGA backend on `bridge` (SR-FW-05). If the bridge is not ready, returns its
/// failed step without touching the keyboard (SF-02). Otherwise starts the Input Manager
/// (SR-INP-01, SR-INP-16) and returns an FpgaBackend whose keyboardPresent() reports whether a
/// keyboard was found. `clock`, `input` and `bindings` must outlive the backend.
BackendStart startFpgaBackend(const BridgeOpen& bridge, hal::MonotonicClock& clock,
                              InputManager& input, const KeyBindings& bindings);

}  // namespace arcade
