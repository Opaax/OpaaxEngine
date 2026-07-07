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
    public:
        Vector4F ClearColor{0, 0, 0, 1.0f};
    };
    
    // Pure, tolerant parser — bad JSON / missing fields keep the defaults, never throws.
    OPAAX_API RendererConfigData ParseRendererConfig(const OpaaxString& InJsonText);
    // Serialize to pretty JSON — the template written when no config file exists.
    OPAAX_API OpaaxString       SerializeRendererConfig(const RendererConfigData& InData);
    
    DECLARE_T_CONFIG_CODEC(ParseRendererConfig, SerializeRendererConfig, RendererConfigData)
}
