/// @file
/// Hands the checked /dev/mem bridge window to startFpgaBackend() (SR-HAL-01, SR-HAL-09, SF-02).
#pragma once

#include <string>

#include "app/fpga_start.hpp"
#include "hal/dev_mem_register_space.hpp"

namespace arcade {

/// Runs `space.open()` (the SR-HAL-09 readiness check, then the SR-HAL-01 mapping) and returns
/// the space in a BridgeOpen, or nullptr with the failed step (SF-02, SR-HAL-02). The step text
/// is stored in `stepText`, which must outlive the returned BridgeOpen.
BridgeOpen openCheckedBridge(hal::DevMemRegisterSpace& space, std::string& stepText);

}  // namespace arcade
