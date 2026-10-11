#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Data/EnemyStats.h"
#include "Resources/DataAsset/DataAssetRef.h"
#include "Engine/Registries/AutoRegistration.h"

namespace Sandbox
{
    // =============================================================================
    // EnemyComponent — an example of a component pointing at a data asset. Many enemies can share one
    //   Data/Grunt.opaaxdata; editing it changes all of them. Only an EnemyStats asset can be dropped on
    //   Stats. Read it at runtime with LoadDataAsset (DataAssetHandle.h).
    // =============================================================================
    struct EnemyComponent
    {
        Opaax::TDataAssetRef<EnemyStats> Stats;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(EnemyComponent, Stats)

        OPAAX_PROPERTIES(EnemyComponent,
                         OPAAX_PROP(Stats).SetTooltip("The EnemyStats asset this enemy uses."))
    };

    OPAAX_REGISTER_COMPONENT(EnemyComponent);
}
