#pragma once

#include "Core/EngineAPI.h"
#include "Movement/Assets/MoveMode.h"

namespace Opaax
{
    // =============================================================================
    // FlyMoveMode — free flight: no gravity, no friction, no jump. Still collides (slides on walls).
    // =============================================================================
    class FlyMoveMode final : public MoveMode
    {
        OPAAX_OBJECT(FlyMoveMode, MoveMode)

        // =============================================================================
        // Tuning
        // =============================================================================
    public:
        /** Speed at full input, world units/s, on both axes. */
        float MaxSpeed = 400.f;

        OPAAX_PROPERTIES(FlyMoveMode, OPAAX_PROP(MaxSpeed).SetRange(0.f, 5000.f))

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin MoveMode interface
        void Tick(MoverTickContext& InContext) const override;

        /** Drops the momentum from the previous mode. */
        void OnModeEnter(MoverTickContext& InContext) const override;
        //~End MoveMode interface
    };
}
