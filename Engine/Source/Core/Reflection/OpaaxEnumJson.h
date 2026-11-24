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
        // A file written before the enum was saved by label holds its number: read it by value.
        if (InJson.is_number_integer())
        {
            const Int64 lNumber = InJson.get<Int64>();

            for (const T lCandidate : TEnumValues<T>::Values)
            {
                if (static_cast<Int64>(lCandidate) == lNumber)
                {
                    InValue = lCandidate;
                    return;
                }
            }

            throw nlohmann::json::type_error::create(
                302, ToJsonString("no enumerator with value " + OpaaxString::FromInt(lNumber) + " in this enum"),
                &InJson);
        }

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
        throw nlohmann::json::type_error::create(
            302, ToJsonString("unknown enumerator '" + lText + "' for this enum"), &InJson);
    }
}

// =============================================================================
// nlohmann looks for to_json/from_json only in the enum's own namespace, so a game's enum would be
// written as its number. This serializer makes nlohmann use the functions above for every enum with
// declared values, whatever its namespace.
// =============================================================================
NLOHMANN_JSON_NAMESPACE_BEGIN
    template<::Opaax::CEnumWithValues T>
    struct adl_serializer<T, void>
    {
        static void to_json(::nlohmann::json& OutJson, const T& InValue)
        {
            ::Opaax::to_json(OutJson, InValue);
        }

        static void from_json(const ::nlohmann::json& InJson, T& OutValue)
        {
            ::Opaax::from_json(InJson, OutValue);
        }
    };
NLOHMANN_JSON_NAMESPACE_END
