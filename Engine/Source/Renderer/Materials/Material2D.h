#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/ResourcePath.h"
#include "Resources/ResourcePathJson.h"

namespace Opaax
{
    struct TextureResource;

    // =============================================================================
    // Material2D — how a sprite takes light: a data asset (.opaaxdata) a SpriteComponent points
    //   at, so many sprites share one. Without one, a sprite is lit, flat and does not glow.
    //   Lighting needs an EnvironmentComponent in the level.
    // =============================================================================
    struct Material2D
    {
        /** Lit by the world's lights and ambient. Off: drawn at full brightness (UI-like, glowing signs). */
        bool bLit = true;

        /** Tangent-space normals, Y up (OpenGL convention), same layout as the sprite's image. */
        TResourcePath<TextureResource> NormalMap;

        /** Added light in the sprite's own colours, unaffected by the scene's lights. Black: none. */
        LinearColor EmissiveColor = { 0.f, 0.f, 0.f, 1.f };

        /** Above 1 it is brighter than white (HDR). */
        float EmissiveStrength = 1.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Material2D, bLit, NormalMap, EmissiveColor, EmissiveStrength)

        OPAAX_PROPERTIES(Material2D,
                         OPAAX_PROP(bLit).SetTooltip("Lit by the scene's lights. Off: always full brightness."),
                         OPAAX_PROP(NormalMap).SetTooltip("Bumps for the lights (tangent space, Y up).\n"
                                                          "Same layout as the sprite's image or sheet."),
                         OPAAX_PROP(EmissiveColor).SetTooltip("Glow in the sprite's own colours. Black: none."),
                         OPAAX_PROP(EmissiveStrength).SetRange(0.f, 100.f).SetDragStep(0.05f)
                                                     .SetTooltip("Above 1 it is brighter than white (HDR)."))
    };
}
