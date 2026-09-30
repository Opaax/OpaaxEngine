#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

namespace Sandbox
{
    // =============================================================================
    // HealthComponent — a game component, defined and registered in the game module, unknown to the
    //   engine. No base class: satisfying CComponent is the whole contract.
    // =============================================================================
    struct HealthComponent
    {
        int Current = 100;
        int Max     = 100;

        // _WITH_DEFAULT so a field added later does not break maps saved before it.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(HealthComponent, Current, Max)

        // Makes it editable in the Inspector.
        OPAAX_PROPERTIES(HealthComponent,
                         OPAAX_PROP(Current),
                         OPAAX_PROP(Max))
    };
}
