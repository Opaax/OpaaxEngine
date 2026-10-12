#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxEnum.h"
#include "Core/String/OpaaxStringJson.h"

namespace Opaax
{
    // =============================================================================
    // JSON for any enum with declared values. Writes the label, not the number,
    // so reordering an enum is safe.
    // =============================================================================
    template<CEnumWithValues T>
    void to_json(nlohmann::json& InJson, const T& InValue)
    {
        InJson = ToString(InValue);
    }

    template<CEnumWithValues T>
    void from_json(const nlohmann::json& InJson, T& InValue)
    {
        const OpaaxString lText = InJson.get<OpaaxString>();

        for (const T lCandidate : TEnumValues<T>::Values)
        {
            if (lText == ToString(lCandidate))
            {
                InValue = lCandidate;
                return;
            }
        }

        // Throws on an unknown label; TConfig::Load catches it and keeps the defaults.
        const OpaaxString lMessage = "unknown enumerator '" + lText + "' for this enum";
        throw nlohmann::json::type_error::create(
            302, nlohmann::json::string_t(lMessage.CStr(), lMessage.GetLength()), &InJson);
    }
}
