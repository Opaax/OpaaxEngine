#pragma once

#include "Core/Systems/Movement/IMoverModeParams.h"

namespace Opaax
{
    // =============================================================================
    // GroundMoveParams
    // =============================================================================
    /**
     * @struct GroundMoveParams
     *
     * Tuning for GroundMoveMode (Quake-style ground/air walk). GravityScale multiplies the SHARED
     * world gravity vector (like RigidbodyComponent::GravityScale) — direction comes from the world,
     * 0 = floats, 2 = heavy. GroundDeceleration is a friction RATE (1/s), not a Box2D surface
     * coefficient (which is why it isn't 0..1).
     */
    struct OPAAX_API GroundMoveParams final : public IMoverModeParams
    {
        float GravityScale      = 1.f;    // × world gravity (shared vector, direction included)
        float MaxSpeed          = 400.f;  // target horizontal speed at full input (world units/s)
        float Acceleration      = 10.f;   // accel responsiveness multiplier (Quake pmove)
        float GroundDeceleration = 8.f;   // ground friction RATE (1/s) — bleeds speed when grounded
        float StopSpeed         = 100.f;  // floor under which friction applies at a fixed rate
        float MinSpeed          = 10.f;   // below this the mover snaps to rest
        float AirSteer          = 0.3f;   // [0..1] fraction of ground accel available in air
        float JumpSpeed         = 500.f;  // upward velocity set on a grounded jump

        OPAAX_MOVER_PARAMS_TYPE(GroundMoveParams)

        nlohmann::json              ToJson() const override;
        void                        FromJson(const nlohmann::json& InJson) override;
        UniquePtr<IMoverModeParams> Clone() const override { return MakeUnique<GroundMoveParams>(*this); }
        void                        DrawEditor() override;
    };

} // namespace Opaax
