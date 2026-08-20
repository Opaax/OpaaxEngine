#pragma once

#include <nlohmann/json.hpp>

#include "Core/Color/LinearColor.h"
#include "Core/Color/LinearColorJson.h"
#include "Core/EngineAPI.h"
#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax
{
    // =============================================================================
    // RendererConfigData — <ProjectRoot>/Configs/Renderer.config.
    //
    //   One field, and it is a LinearColor rather than a Vector4F: the type is what makes the editor
    //   show a colour picker, and its json is the vector's, so the file does not notice.
    //
    //   NeedRestart because RendererManager::Startup copies it into a RenderSystemDesc once.
    // =============================================================================
    struct RendererConfigData
    {
        LinearColor ClearColor{0.0f, 0.0f, 0.0f, 1.0f};

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RendererConfigData, ClearColor)

        OPAAX_PROPERTIES(RendererConfigData,
                         OPAAX_PROP(ClearColor).SetFlags(EPropertyFlags::NeedRestart))
    };
}
