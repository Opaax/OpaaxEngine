#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for OpaaxString, SPLIT from the type the way OpaaxTagJson.h and
    // MathsJson.hpp are: most of the engine wants strings without a json library behind them.
    //
    // It did not exist until configs started serializing through the same macro components use —
    // every config had been hand-converting with .CStr() and get<std::string>().c_str() at each
    // field, which is precisely the per-field labour the macro removes.
    //
    // std::string is the boundary type nlohmann demands, and I13 already names that conversion as
    // the reason the vendor forms survive at all; nothing above this line sees one.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxString& InValue)
    {
        InJson = std::string(InValue.CStr(), InValue.GetLength());
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxString& InValue)
    {
        // Throws type_error on a non-string, which TConfig::Load catches — tolerance lives there,
        // once, rather than in every field of every config.
        const std::string lText = InJson.get<std::string>();

        InValue = OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
    }
}
