#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/Asset.h"

namespace Sandbox
{
    // =============================================================================
    // WeaponStats — a second example asset class (Data/Blaster.opaaxasset). A field asking for
    //   EnemyStats refuses it.
    // =============================================================================
    class WeaponStats final : public Opaax::Asset
    {
        OPAAX_OBJECT(WeaponStats, Opaax::Asset)

    public:
        Opaax::Int32 Damage      = 10;
        float        ShotsPerSec = 4.f;
        float        Spread      = 0.f;

        OPAAX_PROPERTIES(WeaponStats,
                         OPAAX_PROP(Damage).SetRange(0.f, 1000.f),
                         OPAAX_PROP(ShotsPerSec).SetRange(0.1f, 60.f).SetDragStep(0.1f),
                         OPAAX_PROP(Spread).SetRange(0.f, 90.f).SetTooltip("Degrees, either side."))
    };

    OPAAX_REGISTER_OBJECT(WeaponStats);
}
