#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/Asset.h"

namespace Sandbox
{
    // =============================================================================
    // EnemyStats — an example asset class: one .opaaxasset per kind of enemy (Data/Grunt.opaaxasset).
    //   A class with fields and one registration line — that is the whole setup.
    // =============================================================================
    class EnemyStats final : public Opaax::Asset
    {
        OPAAX_OBJECT(EnemyStats, Opaax::Asset)

    public:
        Opaax::Int32       MaxHealth    = 30;
        float              Speed        = 120.f;
        float              FireInterval = 1.5f;
        bool               bFlying      = false;
        Opaax::LinearColor Tint         = { 1.f, 1.f, 1.f, 1.f };

        OPAAX_PROPERTIES(EnemyStats,
                         OPAAX_PROP(MaxHealth).SetRange(1.f, 10000.f),
                         OPAAX_PROP(Speed).SetRange(0.f, 2000.f).SetDragStep(1.f),
                         OPAAX_PROP(FireInterval).SetRange(0.05f, 10.f).SetDragStep(0.05f)
                                                 .SetTooltip("Seconds between shots."),
                         OPAAX_PROP(bFlying).SetTooltip("Ignores the ground."),
                         OPAAX_PROP(Tint))
    };

    // This line is all it takes to get .opaaxasset files of it, made and edited in the editor.
    OPAAX_REGISTER_OBJECT(EnemyStats);
}
