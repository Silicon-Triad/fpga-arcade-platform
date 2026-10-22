// FROZEN INTERFACE: see AGENTS.md section 6. Do not change without an approved issue.
/// @file
/// Engine-only extension of the Arcade API. Only the Game Engine includes this header; games
/// see only ArcadeApi (SR-FW-03, SR-FW-05).
#pragma once

#include "arcade/arcade_api.hpp"
#include "arcade/types.hpp"

namespace arcade {

/// Arcade API plus the calls only the Game Engine may make. Each backend implements this
/// class; the engine holds an EngineApi& and passes games only an ArcadeApi&, so games cannot
/// change the input context (SR-FW-05, SR-INP-09). No call throws (R-4.1.5).
class EngineApi : public ArcadeApi {
  public:
    /// Sets the current key-binding context for keys read by the next pollInput() (SR-INP-09,
    /// SR-INP-11). The engine calls it whenever the screen changes.
    /// @return InvalidArgument for InputContext::Global, which is never current (SR-INP-09).
    virtual ApiStatus setInputContext(InputContext context) noexcept = 0;
};

}  // namespace arcade
