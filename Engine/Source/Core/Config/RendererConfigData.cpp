#include "RendererConfigData.h"

#include <json.hpp>

namespace Opaax
{
    namespace KEcfg = Opaax_Renderer_Config;

    RendererConfigData ParseRendererConfig(const OpaaxString& InJsonText)
    {
        RendererConfigData lData;
        
        if (InJsonText.IsEmpty())
        {
            return lData;
        }
        
        nlohmann::json lRoot;
        try
        {
            lRoot = nlohmann::json::parse(InJsonText.CStr());
        }
        catch (const nlohmann::json::parse_error&)
        {
            return lData;
        }
        
        if (lRoot.contains(KEcfg::CLEAR_COLOR_KEY) && lRoot[KEcfg::CLEAR_COLOR_KEY].is_array() && lRoot[KEcfg::CLEAR_COLOR_KEY].size() == 4
                    && lRoot[KEcfg::CLEAR_COLOR_KEY][0].is_number() && lRoot[KEcfg::CLEAR_COLOR_KEY][1].is_number()
                    && lRoot[KEcfg::CLEAR_COLOR_KEY][2].is_number() && lRoot[KEcfg::CLEAR_COLOR_KEY][3].is_number())
        {
            lData.ClearColor = {
                lRoot[KEcfg::CLEAR_COLOR_KEY][0].get<float>(), 
                lRoot[KEcfg::CLEAR_COLOR_KEY][1].get<float>(),
                lRoot[KEcfg::CLEAR_COLOR_KEY][2].get<float>(),
                lRoot[KEcfg::CLEAR_COLOR_KEY][3].get<float>(),
            };
        }
        
        return lData;
    }

    OpaaxString SerializeRendererConfig(const RendererConfigData& InData)
    {
        nlohmann::json lRoot;
        lRoot[KEcfg::CLEAR_COLOR_KEY] = { InData.ClearColor.x, InData.ClearColor.y, InData.ClearColor.z, InData.ClearColor.w } ;

        return OpaaxString(lRoot.dump(4).c_str());
    }
}
