#pragma once

#include "Core/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    // =============================================================================
    // FlyMoveMode
    // =============================================================================
    /**
     * @class FlyMoveMode
     *
     * Free-flight mover mode: velocity follows the intent directly (MoveDir * MaxSpeed) with NO
     * gravity and no friction — the capsule still sweeps the world (MoveCapsule), so it slides on
     * walls instead of passing through. Registered under OPAAX_ID("Fly"). Demonstrates that a mode
     * is a drop-in strategy: GroundMove and Fly share the same component, seam, and subsystem.
     * Stateless — reads/writes only the per-entity MoverComponent.
     */
    class FlyMoveMode final : public IMoverMode
    {
    public:
        void Tick(MoverTickContext& InContext) override;
        void OnModeEnter(MoverTickContext& InContext) override;
    };

} // namespace Opaax
