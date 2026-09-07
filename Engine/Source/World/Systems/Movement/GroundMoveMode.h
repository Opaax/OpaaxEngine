#pragma once

#include "Core/EngineAPI.h"
#include "World/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // GroundMoveMode — walking, jumping and falling. The platformer mode.
    //
    //   Quake-style ground/air movement: friction while grounded, acceleration toward a desired
    //   horizontal speed with reduced authority in the air, a jump spent only when grounded, and
    //   the world's own gravity vector scaled per tuning. The geometric solve is the seam's
    //   MoveCapsule, so slopes, steps and walls are collide-and-slide rather than policy.
    // =============================================================================
    class OPAAX_API GroundMoveMode final : public IMoverMode
    {
    public:
        //~Begin IMoverMode interface
        void Tick(MoverTickContext& InContext) override;
        //~End IMoverMode interface
    };
}
