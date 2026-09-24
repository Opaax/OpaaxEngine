#pragma once

#include <nlohmann/json.hpp>

namespace Opaax
{
    // =============================================================================
    // CJsonSerializable — a type nlohmann can write and read by ADL.
    //
    //   ONE statement of the requirement, for the two contracts that had it separately: a component
    //   (CComponent, World/Components/ComponentConcept.hpp) and a config data type (TConfigCodec,
    //   Core/Config/TConfig.hpp). Both are satisfied by one line —
    //   NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT — and there is no reason for the engine to spell
    //   the same requirement two ways.
    //
    //   Stated through the json OBJECT rather than by naming to_json/from_json: the assignment and
    //   get_to are SFINAE'd on those free functions, so a type without them fails the concept here
    //   instead of erroring deep inside a template body.
    // =============================================================================
    template<typename T>
    concept CJsonSerializable = requires(nlohmann::json& InJson, const T& InSource, T& InTarget)
    {
        { InJson = InSource };
        { InJson.get_to(InTarget) };
    };
}
