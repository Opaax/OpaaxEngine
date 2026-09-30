#pragma once

#include <nlohmann/json.hpp>

namespace Opaax
{
    // =============================================================================
    // CJsonSerializable — a type nlohmann can read and write
    //   (e.g. with NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT).
    // =============================================================================
    template<typename T>
    concept CJsonSerializable = requires(nlohmann::json& InJson, const T& InSource, T& InTarget)
    {
        { InJson = InSource };
        { InJson.get_to(InTarget) };
    };
}
