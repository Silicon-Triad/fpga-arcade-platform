/// @file
/// The end of the program: the ordered steps of SR-FW-13 and SR-FW-14.
#pragma once

#include <functional>

#include "arcade/engine_api.hpp"
#include "engine/app.hpp"

namespace arcade {

/// Exit status of a confirmed shutdown: "shutdown requested" (SR-FW-13, SC-01). The program runs
/// without root, so the root start-up script runs `poweroff` when it sees this status (#39).
/// DESIGN CHOICE, NOT AN SRS VALUE: 3 differs from 0, 1 (SF-02) and 2 (bad command line).
constexpr int kExitShutdownRequested = 3;

/// Runs, in order: mute; `savePending`; draw screen 10b and present() it; `release`, which must
/// not clear display enable (SR-HAL-07). `app` enters ShuttingDown, and 10b is drawn only once
/// `savePending` has returned. `api` must not be used after `release`.
/// @param confirmed True after a confirmed shutdown (SR-FW-13), false for a signal or a closed
/// window (SR-FW-14).
/// @return kExitShutdownRequested if `confirmed`, else 0.
int finishShutdown(EngineApi& api, App& app, bool confirmed,
                   const std::function<void()>& savePending, const std::function<void()>& release);

}  // namespace arcade
