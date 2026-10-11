#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Sandbox
{
    // =============================================================================
    // EnemyStats — an example data asset: one .opaaxdata per kind of enemy (Data/Grunt.opaaxdata).
    //   A plain struct registered with DataAssets().Register<EnemyStats>() — that is the whole setup.
    // =============================================================================
    struct EnemyStats
    {
        Opaax::Int32       MaxHealth    = 30;
        float              Speed        = 120.f;
        float              FireInterval = 1.5f;
        bool               bFlying      = false;
        Opaax::LinearColor Tint         = { 1.f, 1.f, 1.f, 1.f };

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EnemyStats, MaxHealth, Speed, FireInterval, bFlying, Tint)

        OPAAX_PROPERTIES(EnemyStats,
                         OPAAX_PROP(MaxHealth).SetRange(1.f, 10000.f),
                         OPAAX_PROP(Speed).SetRange(0.f, 2000.f).SetDragStep(1.f),
                         OPAAX_PROP(FireInterval).SetRange(0.05f, 10.f).SetDragStep(0.05f)
                                                 .SetTooltip("Seconds between shots."),
                         OPAAX_PROP(bFlying).SetTooltip("Ignores the ground."),
                         OPAAX_PROP(Tint))
    };

    // This line is all it takes to get .opaaxdata files of it, editable in the editor.
    OPAAX_REGISTER_DATA_ASSET(EnemyStats);
}
