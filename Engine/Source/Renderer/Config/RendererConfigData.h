#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnumJson.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Renderer/Post/SceneTargetPlan.h"
#include "RHI/TextureSampling.h"

namespace Opaax
{
    // =============================================================================
    // RendererConfigData — <ProjectRoot>/Configs/Renderer.config.
    //   The limits size one draw call (quads and textures per batch); they only change the
    //   draw call count, never the picture. They need a restart. The others apply live.
    // =============================================================================
    struct RendererConfigData
    {
        LinearColor ClearColor{0.0f, 0.0f, 0.0f, 1.0f};

        /** How the project's colour textures are read at other sizes than their own. Fonts are not concerned. */
        ETextureFilter TextureFilter = ETextureFilter::Pixel;

        /** Samples per pixel of the world's target: smooth edges on quads, tiles and shapes. */
        EMultisampling Multisampling = EMultisampling::Off;

        Uint32 MaxQuadsPerBatch = 10000;
        Uint32 MaxTextureSlots  = 16;   // sampler array length in the sprite shader; 2 = white + one

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RendererConfigData, ClearColor, TextureFilter, Multisampling,
                                                    MaxQuadsPerBatch, MaxTextureSlots)

        OPAAX_PROPERTIES(RendererConfigData,
                         OPAAX_PROP(ClearColor),
                         OPAAX_PROP(TextureFilter)
                             .SetTooltip("How sprites and UI images are read when drawn at another size than "
                                         "their pixels. Applies at once, to every texture.\n"
                                         "Nearest: hard pixels at any size. Sharp, but shimmers when drawn "
                                         "smaller or moved slowly.\n"
                                         "Pixel: hard pixels when enlarged, blended when reduced. For pixel "
                                         "art.\n"
                                         "Smooth: blended when enlarged, and reduced through smaller copies "
                                         "of the texture (mipmaps), so small art keeps its thin strokes and "
                                         "does not shimmer. For drawn or painted art. A sheet frame drawn "
                                         "much smaller can take colour from its neighbour unless frames sit "
                                         "on a grid of 8 or 16 texels, or have empty texels between them.\n"
                                         "Costs nothing per frame; the smaller copies take a third more "
                                         "video memory, whatever the choice. Text is not concerned."),
                         OPAAX_PROP(Multisampling)
                             .SetTooltip("Smooths the stair steps on the edges of shapes: a turned quad, a tile, "
                                         "a line, a solid sprite. Applies at once, to the world (not the UI).\n"
                                         "It does not smooth what is cut inside a texture: the edge of a sprite "
                                         "with a transparent border, a hollow quad, a mask. Nor an edge brighter "
                                         "than white in a lit world.\n"
                                         "Costs memory and fill: the world's target holds that many samples per "
                                         "pixel (X4: four times the pixels drawn into). A driver that gives fewer "
                                         "uses its most; renderer.info says how many are used."),
                         OPAAX_PROP(MaxQuadsPerBatch)
                             .SetRange(1.f, 65536.f)
                             .SetFlags(EPropertyFlags::NeedRestart)
                             .SetTooltip("How many quads fit in ONE draw call.\n"
                                         "Sizes a GPU buffer reserved at startup (416 bytes a quad), so this "
                                         "is reserved memory against draw calls: too small and the frame "
                                         "splits constantly, too large and a small scene reserves what it "
                                         "never draws.\n"
                                         "Splitting costs draw calls only — a pass is sorted whole before "
                                         "it is cut, so the picture never changes."),
                         OPAAX_PROP(MaxTextureSlots)
                             .SetRange(2.f, 16.f)
                             .SetFlags(EPropertyFlags::NeedRestart)
                             .SetTooltip("How many DISTINCT textures one draw call may sample.\n"
                                         "Not a free choice: the sprite shader declares sampler2D "
                                         "u_Textures[16] and 16 is OpenGL's guaranteed minimum, so this "
                                         "cannot go higher. The floor of 2 is the white texture plus one.\n"
                                         "Sprites sharing an atlas share a slot, which is what keeps a "
                                         "crowded frame in one draw call."))
    };
}
