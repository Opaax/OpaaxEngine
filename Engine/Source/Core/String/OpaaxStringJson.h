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

    /** nlohmann's own string (an object's key, a value read as text) as an OpaaxString. */
    inline OpaaxString FromJsonString(const nlohmann::json::string_t& InText)
    {
        return OpaaxString(InText.data(), static_cast<Uint32>(InText.size()));
    }

    inline void from_json(const nlohmann::json& InJson, OpaaxString& InValue)
    {
        // Throws type_error on a non-string (TConfig::Load catches it).
        InValue = FromJsonString(InJson.get_ref<const nlohmann::json::string_t&>());
    }

    /** InText as nlohmann's own string, for the json functions that take one (json_pointer, errors). */
    inline nlohmann::json::string_t ToJsonString(const OpaaxStringView InText)
    {
        return InText.IsEmpty() ? nlohmann::json::string_t()
                                : nlohmann::json::string_t(InText.Data(), InText.GetLength());
    }

    /** InJson as text: on one line, or indented by InIndent spaces. */
    inline OpaaxString ToJsonText(const nlohmann::json& InJson, const Int32 InIndent = -1)
    {
        return FromJsonString(InJson.dump(InIndent));
    }
}
