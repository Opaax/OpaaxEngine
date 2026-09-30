#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // JSON for OpaaxString. Kept apart so OpaaxString does not pull in json.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxString& InValue)
    {
        InJson = std::string(InValue.CStr(), InValue.GetLength());
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxString& InValue)
    {
        // Throws type_error on a non-string (TConfig::Load catches it).
        const std::string lText = InJson.get<std::string>();

        InValue = OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size()));
    }
}
