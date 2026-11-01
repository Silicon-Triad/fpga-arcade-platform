#include "engine/shutdown.hpp"

namespace arcade {

int finishShutdown(EngineApi& api, App& app, bool confirmed,
                   const std::function<void()>& savePending, const std::function<void()>& release) {
    app.setState(AppState::ShuttingDown);
    // SR-FW-13, SR-FW-14: this order. The program never runs poweroff itself (SC-01).
    api.setMute(true);
    savePending();
    app.shuttingDown().saveFinished();
    app.render(api);
    (void)api.present();  // a failed swap must not stop the shutdown (R-4.1.5)
    release();
    return confirmed ? kExitShutdownRequested : 0;
}

}  // namespace arcade
