#pragma once
#include "Core/EngineAPI.h"
#include <Core/OpaaxString.hpp>

#include "TConfig.hpp"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    namespace Opaax_Renderer_Config
    {
        inline const char* CLEAR_COLOR_KEY    = "clear_color";
    }
    
    struct RendererConfigData
    {
        DECLARE_CONFIG_DATA(RendererConfigData);
    public:
        Vector4F ClearColor{0, 0, 0, 1.0f};
    };
    
    DECLARE_T_CONFIG_CODEC(RendererConfigData::Parse, RendererConfigData::Serialize, RendererConfigData)
}
