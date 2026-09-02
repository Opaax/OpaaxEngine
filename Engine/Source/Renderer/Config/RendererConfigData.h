#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // RendererConfigData — <ProjectRoot>/Configs/Renderer.config.
    //
    //   ClearColor is a LinearColor rather than a Vector4F: the type is what makes the editor show
    //   a colour picker, and its json is the vector's, so the file does not notice.
    //
    //   The two limits size ONE draw call — how many quads and how many distinct textures fit
    //   before the frame splits. They are a cost knob only: a pass is sorted whole before it is
    //   cut, so shrinking them changes the draw call count and never the picture, which is exactly
    //   what makes the split path reachable in a small scene.
    //
    //   All NeedRestart because RendererManager::Startup copies them into a RenderSystemDesc once.
    // =============================================================================
    struct RendererConfigData
    {
        LinearColor ClearColor{0.0f, 0.0f, 0.0f, 1.0f};

        Uint32 MaxQuadsPerBatch = 1000;
        Uint32 MaxTextureSlots  = 16;   // the sprite shader's sampler array length; 2 = white + one

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RendererConfigData, ClearColor,
                                                    MaxQuadsPerBatch, MaxTextureSlots)

        OPAAX_PROPERTIES(RendererConfigData,
                         OPAAX_PROP(ClearColor).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(MaxQuadsPerBatch).SetRange(1.f, 65536.f).SetFlags(EPropertyFlags::NeedRestart),
                         OPAAX_PROP(MaxTextureSlots).SetRange(2.f, 16.f).SetFlags(EPropertyFlags::NeedRestart))
    };
}
