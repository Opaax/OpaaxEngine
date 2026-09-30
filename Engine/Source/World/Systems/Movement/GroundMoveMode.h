#pragma once

#include "Core/EngineAPI.h"
#include "World/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // GroundMoveMode — walking, jumping and falling (platformer).
    //   Quake-style: friction on the ground, acceleration with less control in the air, jump only
    //   when grounded, and gravity. MoveCapsule handles slopes, steps and walls.
    // =============================================================================
    class OPAAX_API GroundMoveMode final : public IMoverMode
    {
    public:
        //~Begin IMoverMode interface
        void Tick(MoverTickContext& InContext) override;
        //~End IMoverMode interface
    };
}
