#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"

namespace Opaax
{
    // =============================================================================
    // JSON for Guid: 32 lowercase hex characters. Kept apart from Guid.h so it does not pull in json.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const Guid& InValue)
    {
        InJson = InValue.ToString().CStr();
    }

    inline void from_json(const nlohmann::json& InJson, Guid& InValue)
    {
        // Throws type_error on a non-string (caught by MapFactory::Instantiate).
        std::string lText;
        InJson.get_to(lText);

        // A malformed string gives an invalid Guid instead of throwing.
        Guid lParsed;
        InValue = Guid::FromString(OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size())), lParsed)
                      ? lParsed
                      : Guid{};
    }
}
