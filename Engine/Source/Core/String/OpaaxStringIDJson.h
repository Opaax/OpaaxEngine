#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // JSON for OpaaxStringID. Writes the text, never the id (ids change between runs).
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxStringID& InValue)
    {
        // An invalid id writes "" (not "None").
        InJson = InValue.IsValid() ? std::string(InValue.CStr()) : std::string();
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxStringID& InValue)
    {
        // Throws type_error on a non-string (caught by the caller).
        const std::string lText = InJson.get<std::string>();

        InValue = lText.empty() ? OpaaxStringID() : OpaaxStringID(lText);
    }
}
