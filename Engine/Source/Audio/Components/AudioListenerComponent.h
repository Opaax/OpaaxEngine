#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // AudioListenerComponent — where spatial sounds are heard from (the player, a camera rig).
    //   The first active one is used; without one, the centre of the world's camera.
    // =============================================================================
    struct AudioListenerComponent
    {
        bool bActive = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioListenerComponent, bActive)

        OPAAX_PROPERTIES(AudioListenerComponent,
                         OPAAX_PROP(bActive).SetTooltip("Only an active listener is used."))
    };
}
