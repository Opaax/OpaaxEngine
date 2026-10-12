#pragma once

#include <nlohmann/json.hpp>

#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // JSON for OpaaxString. Kept apart so OpaaxString does not pull in json. The other JSON
    //   conversions of text (ids, tags, guids, enums) go through these.
    // =============================================================================
    inline void to_json(nlohmann::json& InJson, const OpaaxString& InValue)
    {
        InJson = nlohmann::json::string_t(InValue.CStr(), InValue.GetLength());
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxString& InValue)
    {
        // Throws type_error on a non-string (TConfig::Load catches it).
        const nlohmann::json::string_t& lText = InJson.get_ref<const nlohmann::json::string_t&>();

        InValue = OpaaxString(lText.data(), static_cast<Uint32>(lText.size()));
    }

    /** InJson as text: on one line, or indented by InIndent spaces. */
    inline OpaaxString ToJsonText(const nlohmann::json& InJson, const Int32 InIndent = -1)
    {
        const nlohmann::json::string_t lText = InJson.dump(InIndent);

        return OpaaxString(lText.data(), static_cast<Uint32>(lText.size()));
    }
}
