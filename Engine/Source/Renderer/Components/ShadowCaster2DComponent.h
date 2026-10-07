#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // ShadowCaster2DComponent — the entity's sprite or quad blocks the lights that cast shadows,
    //   by its silhouette (its alpha). Shadows need an EnvironmentComponent in the level and a
    //   Light2D with bCastShadows.
    // =============================================================================
    struct ShadowCaster2DComponent
    {
        /** On: shadowed like any sprite, its own shape included. Off: lit as if nothing blocked the light. */
        bool bSelfShadows = false;

        bool bEnabled = true;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ShadowCaster2DComponent, bSelfShadows, bEnabled)

        OPAAX_PROPERTIES(ShadowCaster2DComponent,
                         OPAAX_PROP(bSelfShadows).SetTooltip("On: shadowed like any sprite, its own shape included\n"
                                                             "(only the edge facing a light stays lit).\n"
                                                             "Off: lit as if nothing blocked the light."),
                         OPAAX_PROP(bEnabled))
    };
}
