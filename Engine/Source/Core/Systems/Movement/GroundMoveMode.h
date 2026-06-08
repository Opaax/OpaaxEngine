#pragma once

#include "Core/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // GroundMoveMode
    // =============================================================================
    /**
     * @class GroundMoveMode
     *
     * The one built-in mover mode: walk / fall / jump / slide on the ground (Quake-style
     * ground+air policy — friction when grounded, accelerate toward intent with reduced air
     * control, grounded jump impulse, constant gravity), then a geometric capsule sweep
     * (World.MoveCapsule) for collide-and-slide. Registered under OPAAX_ID("GroundMove").
     * Stateless — reads/writes only the per-entity MoverComponent.
     */
    class GroundMoveMode final : public IMoverMode
    {
    public:
        UniquePtr<IMoverModeParams> CreateDefaultParams() const override;
        void                        Tick(MoverTickContext& InContext) override;
    };

} // namespace Opaax
