#pragma once

#include <nlohmann/json.hpp>

#include "Core/Maths/Maths.h"
#include "Core/String/CString.h"
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

    /**
     * InValue written back as the shortest decimal of the float it holds (0.4f is stored as
     * 0.4000000059604645, written here as 0.4), when it is exactly a float; any other double as is.
     * A float field reads either back as the same float.
     */
    inline double ShortestFloatValue(const double InValue) noexcept
    {
        const float lFloat = static_cast<float>(InValue);
        if (!Maths::IsFinite(lFloat) || static_cast<double>(lFloat) != InValue)
        {
            return InValue;
        }

        char lText[32];
        CString::Format(lText, "{}", lFloat);   // fmt writes a float's shortest round-trip form
        return CString::ToDouble(lText);
    }

    /** Every float in InOutJson, objects and arrays included, through ShortestFloatValue. */
    inline void TidyJsonFloats(nlohmann::json& InOutJson)
    {
        if (InOutJson.is_number_float())
        {
            InOutJson = ShortestFloatValue(InOutJson.get<double>());
            return;
        }

        if (InOutJson.is_structured())
        {
            for (nlohmann::json& lChild : InOutJson)
            {
                TidyJsonFloats(lChild);
            }
        }
    }

    /** InText as nlohmann's own string, for the json functions that take one (json_pointer, errors). */
    inline nlohmann::json::string_t ToJsonString(const OpaaxStringView InText)
    {
        return InText.IsEmpty() ? nlohmann::json::string_t()
                                : nlohmann::json::string_t(InText.Data(), InText.GetLength());
    }

    /**
     * InJson as text: on one line, or indented by InIndent spaces. Floats are written in their
     * shortest form (TidyJsonFloats), so a saved file keeps the values as they were typed.
     */
    inline OpaaxString ToJsonText(const nlohmann::json& InJson, const Int32 InIndent = -1)
    {
        nlohmann::json lTidy = InJson;
        TidyJsonFloats(lTidy);
        return FromJsonString(lTidy.dump(InIndent));
    }
}
