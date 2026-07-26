#pragma once
#include "Core/EngineAPI.h"
#include <Core/String/OpaaxString.hpp>

#include "Core/Config/TConfig.hpp"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    namespace Opaax_Renderer_Config
    {
        inline const char* CLEAR_COLOR_KEY    = "clear_color";
    }
    
    struct RendererConfigData
    {
        DECLARE_CONFIG_DATA(RendererConfigData)
    public:
        Vector4F ClearColor{0.0f, 0.0f, 0.0f, 1.0f};
    };
    
    DECLARE_T_CONFIG_CODEC(RendererConfigData::Parse, RendererConfigData::Serialize, RendererConfigData)
}
