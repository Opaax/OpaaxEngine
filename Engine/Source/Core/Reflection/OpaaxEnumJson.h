#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxEnum.h"

namespace Opaax
{
    // =============================================================================
    // The nlohmann bridge for ANY enum that declared its values — one template, not one pair of
    // functions per enum, and split from the type the way every other bridge here is.
    //
    //   It writes the LABEL, never the ordinal. That is what keeps a config readable and, more
    //   importantly, what makes reordering an enum safe: an ordinal on disk silently means a
    //   different enumerator the day someone inserts one, which is the exact failure this codebase
    //   spends invariants avoiding. It is also why converting a stringly-typed config field to an
    //   enum leaves the file byte-identical.
    // =============================================================================
    template<CEnumWithValues T>
    void to_json(nlohmann::json& InJson, const T& InValue)
    {
        InJson = ToString(InValue);
    }

    template<CEnumWithValues T>
    void from_json(const nlohmann::json& InJson, T& InValue)
    {
        const std::string lText = InJson.get<std::string>();

        for (const T lCandidate : TEnumValues<T>::Values)
        {
            if (lText == ToString(lCandidate))
            {
                InValue = lCandidate;
                return;
            }
        }

        // THROWS, and that is consistency rather than a new policy: since BO1b a wrong-typed value
        // anywhere in a config throws, TConfig::Load catches it, the defaults stand and ConfigSystem
        // warns naming the file. A misspelled enumerator is now the same event as a misspelled
        // number — and the editor's dropdown cannot produce one in the first place.
        throw nlohmann::json::type_error::create(
            302, "unknown enumerator '" + lText + "' for this enum", &InJson);
    }
}
