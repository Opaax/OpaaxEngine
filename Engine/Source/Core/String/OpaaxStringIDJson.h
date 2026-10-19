#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    // =============================================================================
    // JSON for OpaaxStringID. Writes the text, never the id (ids change between runs).
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxStringID& InValue)
    {
        // An invalid id writes "" (not "None").
        InJson = InValue.IsValid() ? InValue.ToString() : OpaaxString();
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxStringID& InValue)
    {
        // Throws type_error on a non-string (caught by the caller).
        const OpaaxString lText = InJson.get<OpaaxString>();

        InValue = lText.IsEmpty() ? OpaaxStringID() : OpaaxStringID(lText);
    }
}
