#pragma once

#include "Core/EngineAPI.h"
#include "Movement/Modes/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // FlyMoveMode — free flight: no gravity, no friction, no jump. Still collides (slides on walls).
    // =============================================================================
    class FlyMoveMode final : public IMoverMode
    {
    public:
        //~Begin IMoverMode interface
        void Tick(MoverTickContext& InContext) override;

        /** Drops the momentum from the previous mode. */
        void OnModeEnter(MoverTickContext& InContext) override;
        //~End IMoverMode interface
    };
}
