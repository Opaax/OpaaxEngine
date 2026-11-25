#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Renderer/Materials/SpriteShader.h"   // complete: an asset class visits its shader reference
#include "Renderer/Textures/Texture2D.h"        // complete: an asset class visits its texture reference
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetRef.h"

namespace Opaax
{
    // =============================================================================
    // Material2D — how a sprite is drawn: an asset a SpriteComponent points at, so many sprites share
    //   one. Without one, a sprite is lit, flat, does not glow and uses the engine's sprite shader.
    //   Lighting needs an EnvironmentComponent in the level. A Shader draws its sprites with a game's
    //   own shader, which reads the parameters, colours and textures below (MaterialUBO).
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

        /** The game's shader its sprites are drawn with. Empty, or one that does not compile: the engine's. */
        TAssetRef<SpriteShader> Shader;

        /** Numbers the shader reads: Param0..3 in u_Params[0].xyzw, Param4..7 in u_Params[1].xyzw. */
        float Param0 = 0.f;
        float Param1 = 0.f;
        float Param2 = 0.f;
        float Param3 = 0.f;
        float Param4 = 0.f;
        float Param5 = 0.f;
        float Param6 = 0.f;
        float Param7 = 0.f;

        /** Colours the shader reads, as written (screen colour): u_Colors[0] and u_Colors[1]. */
        LinearColor Color0 = { 1.f, 1.f, 1.f, 1.f };
        LinearColor Color1 = { 1.f, 1.f, 1.f, 1.f };

        /** Textures the shader reads, in u_Textures[12] and u_Textures[13]. Empty: white. */
        TAssetRef<Texture2D> Texture0;
        TAssetRef<Texture2D> Texture1;

        OPAAX_PROPERTIES(Material2D,
                         OPAAX_PROP(bLit).SetTooltip("Lit by the scene's lights. Off: always full brightness."),
                         OPAAX_PROP(NormalMap).SetTooltip("Bumps for the lights (tangent space, Y up).\n"
                                                          "Same layout as the sprite's image or sheet."),
                         OPAAX_PROP(EmissiveColor).SetTooltip("Glow in the sprite's own colours. Black: none."),
                         OPAAX_PROP(EmissiveStrength).SetRange(0.f, 100.f).SetDragStep(0.05f)
                                                     .SetTooltip("Above 1 it is brighter than white (HDR),\n"
                                                                 "and blooms when the Environment has bloom."),
                         OPAAX_PROP(Shader).SetTooltip("A game's sprite shader (.glsl), drawn instead of the engine's.\n"
                                                       "Start from Engine/Assets/Shaders/SpriteMaterial.glsl.\n"
                                                       "One that does not compile: the engine's, with an error in the log."),
                         OPAAX_PROP(Param0).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[0].x"),
                         OPAAX_PROP(Param1).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[0].y"),
                         OPAAX_PROP(Param2).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[0].z"),
                         OPAAX_PROP(Param3).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[0].w"),
                         OPAAX_PROP(Param4).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[1].x"),
                         OPAAX_PROP(Param5).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[1].y"),
                         OPAAX_PROP(Param6).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[1].z"),
                         OPAAX_PROP(Param7).SetDragStep(0.01f).SetTooltip("For the shader: u_Params[1].w"),
                         OPAAX_PROP(Color0).SetTooltip("For the shader: u_Colors[0], as picked (screen colour)"),
                         OPAAX_PROP(Color1).SetTooltip("For the shader: u_Colors[1], as picked (screen colour)"),
                         OPAAX_PROP(Texture0).SetTooltip("For the shader: u_Textures[12]. Empty: white."),
                         OPAAX_PROP(Texture1).SetTooltip("For the shader: u_Textures[13]. Empty: white."))
    };
}
