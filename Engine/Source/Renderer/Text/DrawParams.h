#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"

#include <nlohmann/json.hpp>

namespace Opaax::Text2D
{
    using json = nlohmann::json;

    // =============================================================================
    // DrawParams
    //
    // Tunable knobs for Text2D::DrawString / Measure. 
    // POD with nlohmann ADL hooks day one so a future Text
    // Style asset can serialize presets. Deliberately minimal (4 fields per D-c
    // lock): anchor / rotation / letter-spacing / word-wrap belong on the future
    // HUD and Draw Debug API milestones, not on this primitive.
    // =============================================================================
    /**
     * @struct DrawParams
     *
     * Color tint multiplied into R8 coverage; Scale multiplies the atlas-baked
     * pixel size; LineHeightScale multiplies the font's natural \n advance;
     * EnableKerning short-circuits the kerning LUT lookup when false.
     */
    struct OPAAX_API DrawParams
    {
        Vector4F Color           = Vector4F(1.f);   // RGBA, multiplied into glyph coverage
        float    Scale           = 1.f;             // 1.0 = atlas pixel size (32 px per D-g bake)
        float    LineHeightScale = 1.f;             // multiplies font LineAdvance for '\n'
        bool     EnableKerning   = true;            // false skips GetKerning lookup
    };

    // =============================================================================
    // nlohmann ADL
    // =============================================================================
    inline void ToJson(json& OutJson, const DrawParams& InParams)
    {
        OutJson = json{
            { "color",            { InParams.Color.r, InParams.Color.g, InParams.Color.b, InParams.Color.a } },
            { "scale",            InParams.Scale },
            { "lineHeightScale",  InParams.LineHeightScale },
            { "enableKerning",    InParams.EnableKerning }
        };
    }

    inline void FromJson(const json& InJson, DrawParams& OutParams)
    {
        if (InJson.contains("color"))
        {
            OutParams.Color.r = InJson["color"][0].get<float>();
            OutParams.Color.g = InJson["color"][1].get<float>();
            OutParams.Color.b = InJson["color"][2].get<float>();
            OutParams.Color.a = InJson["color"][3].get<float>();
        }
        if (InJson.contains("scale"))
        {
            OutParams.Scale = InJson["scale"].get<float>();
        }
        if (InJson.contains("lineHeightScale"))
        {
            OutParams.LineHeightScale = InJson["lineHeightScale"].get<float>();
        }
        if (InJson.contains("enableKerning"))
        {
            OutParams.EnableKerning = InJson["enableKerning"].get<bool>();
        }
    }

} // namespace Opaax::Text2D
