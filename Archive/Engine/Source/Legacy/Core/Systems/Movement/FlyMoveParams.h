#pragma once

#include "Core/Systems/Movement/IMoverModeParams.h"

namespace Opaax
{
    // =============================================================================
    // FlyMoveParams
    // =============================================================================
    /**
     * @struct FlyMoveParams
     *
     * Tuning for FlyMoveMode (free flight). Carries ONLY MaxSpeed — no gravity/friction/jump,
     * which is the whole point of per-mode params: a mode holds only the knobs it uses.
     */
    struct OPAAX_API FlyMoveParams final : public IMoverModeParams
    {
        float MaxSpeed = 400.f;   // velocity at full directional input (world units/s)

        OPAAX_MOVER_PARAMS_TYPE(FlyMoveParams)

        nlohmann::json              ToJson() const override { return { { "max_speed", MaxSpeed } }; }
        void                        FromJson(const nlohmann::json& InJson) override
        {
            if (InJson.contains("max_speed")) { MaxSpeed = InJson["max_speed"].get<float>(); }
        }
        UniquePtr<IMoverModeParams> Clone() const override { return MakeUnique<FlyMoveParams>(*this); }
        void                        DrawEditor() override;
    };

} // namespace Opaax
