#pragma once

#include "Core/EngineAPI.h"
#include "World/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // FlyMoveMode — free flight. No gravity, no friction, no jump.
    //
    //   Intent maps straight to velocity; the capsule sweep still resolves collisions, so a flying
    //   thing slides along walls rather than passing through them. It is the SECOND mode, and its
    //   real job is being one: a mover with two entries proves the switch, the transition hooks
    //   and the per-mode tuning are all load-bearing rather than asserted.
    // =============================================================================
    class OPAAX_API FlyMoveMode final : public IMoverMode
    {
    public:
        //~Begin IMoverMode interface
        void Tick(MoverTickContext& InContext) override;

        /** Drop momentum carried in from the previous mode — a fall does not continue into flight. */
        void OnModeEnter(MoverTickContext& InContext) override;
        //~End IMoverMode interface
    };
}
