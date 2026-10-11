#pragma once

#include <nlohmann/json.hpp>

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Sandbox
{
    // =============================================================================
    // WeaponStats — a second example data asset (Data/Blaster.opaaxdata). A field asking for
    //   EnemyStats refuses it.
    // =============================================================================
    struct WeaponStats
    {
        Opaax::Int32 Damage      = 10;
        float        ShotsPerSec = 4.f;
        float        Spread      = 0.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(WeaponStats, Damage, ShotsPerSec, Spread)

        OPAAX_PROPERTIES(WeaponStats,
                         OPAAX_PROP(Damage).SetRange(0.f, 1000.f),
                         OPAAX_PROP(ShotsPerSec).SetRange(0.1f, 60.f).SetDragStep(0.1f),
                         OPAAX_PROP(Spread).SetRange(0.f, 90.f).SetTooltip("Degrees, either side."))
    };

    OPAAX_REGISTER_DATA_ASSET(WeaponStats);
}
