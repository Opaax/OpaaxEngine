#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Renderer/Textures/Texture2D.h"   // complete: an asset class visits its texture reference
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    // =============================================================================
    // Material2D — how a sprite takes light: an asset a SpriteComponent points at, so many sprites
    //   share one. Without one, a sprite is lit, flat and does not glow. Lighting needs an
    //   EnvironmentComponent in the level.
    // =============================================================================
    class Material2D final : public Asset
    {
        OPAAX_OBJECT(Material2D, Asset)

    public:
        /** Lit by the world's lights and ambient. Off: drawn at full brightness (UI-like, glowing signs). */
        bool bLit = true;

        /** Tangent-space normals, Y up (OpenGL convention), same layout as the sprite's image. */
        TAssetRef<Texture2D> NormalMap;

        /** Added light in the sprite's own colours, unaffected by the scene's lights. Black: none. */
        LinearColor EmissiveColor = { 0.f, 0.f, 0.f, 1.f };

        /** Above 1 it is brighter than white (HDR), and blooms when the Environment has bloom. */
        float EmissiveStrength = 1.f;

        OPAAX_PROPERTIES(Material2D,
                         OPAAX_PROP(bLit).SetTooltip("Lit by the scene's lights. Off: always full brightness."),
                         OPAAX_PROP(NormalMap).SetTooltip("Bumps for the lights (tangent space, Y up).\n"
                                                          "Same layout as the sprite's image or sheet."),
                         OPAAX_PROP(EmissiveColor).SetTooltip("Glow in the sprite's own colours. Black: none."),
                         OPAAX_PROP(EmissiveStrength).SetRange(0.f, 100.f).SetDragStep(0.05f)
                                                     .SetTooltip("Above 1 it is brighter than white (HDR),\n"
                                                                 "and blooms when the Environment has bloom."))
    };
}
