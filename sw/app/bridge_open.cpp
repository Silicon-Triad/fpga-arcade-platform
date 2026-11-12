#include "app/bridge_open.hpp"

namespace arcade {

BridgeOpen openCheckedBridge(hal::DevMemRegisterSpace& space, std::string& stepText) {
    const hal::BridgeResult result = space.open();
    if (!result.ok()) {
        // SF-02, SR-HAL-09: startFpgaBackend() reports "graphics hardware not ready" + this step.
        stepText = hal::describeBridgeFailure(result, space.readiness());
        return BridgeOpen{nullptr, stepText};
    }
    return BridgeOpen{&space, {}};
}

}  // namespace arcade
