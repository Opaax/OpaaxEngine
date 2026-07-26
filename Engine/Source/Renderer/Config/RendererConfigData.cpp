#include "RendererConfigData.h"

#include <json.hpp>
#include "Core/Maths/MathsJson.hpp"

namespace Opaax
{
    namespace KEcfg = Opaax_Renderer_Config;

    RendererConfigData RendererConfigData::Parse(const OpaaxString& InJsonText)
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
        catch (const nlohmann::json::parse_error& ParseError)
        {
            return lData;
        }
        
        //ClearColor
        if (lRoot.contains(KEcfg::CLEAR_COLOR_KEY))
        {
            try 
            {
                lData.ClearColor = lRoot[KEcfg::CLEAR_COLOR_KEY].get<Vector4F>();
            } 
            catch (const std::exception& Excep) 
            {
            }
        }
        
        return lData;
    }

    OpaaxString RendererConfigData::Serialize(const RendererConfigData& InData)
    {
        nlohmann::json lRoot;
        
        //ClearColor
        lRoot[KEcfg::CLEAR_COLOR_KEY] = InData.ClearColor ;

        return OpaaxString(lRoot.dump(4).c_str());
    }
}
