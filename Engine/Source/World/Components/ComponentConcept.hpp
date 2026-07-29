#pragma once

#include <type_traits>

#include <nlohmann/json.hpp>

// =============================================================================
// CComponent — THE compile-time contract for a serializable component.
//
//   Same shape as CResource (Engine/Subsystems/Resources/ResourceConcept.hpp): a concept
//   instead of a base class, because entt stores components BY VALUE — so Save/Load cannot
//   be member virtuals the way the retired Legacy ComponentRegistry did it. The contract has
//   to be external to the type.
//
//   THERE IS NO COMPONENT BASE CLASS, on purpose. An empty marker base used to exist; it was
//   deleted once this concept took over its stated job, because an empty non-virtual base is
//   an attractive nuisance — the first person to add a virtual to it silently breaks entt's
//   by-value storage, and nothing would complain. No base, no vtable to add.
//
//   The contract is nlohmann's free-function pair, found by ADL:
//       void to_json  (nlohmann::json&, const T&);
//       void from_json(const nlohmann::json&, T&);
//   which is the idiom already used for glm vectors (Core/Maths/MathsJson.hpp) and the
//   config data types. `NLOHMANN_DEFINE_TYPE_INTRUSIVE(T, fields...)` generates both in one
//   line, so a game component costs one macro and ZERO engine registration beyond naming it.
// =============================================================================
namespace Opaax
{
    template<typename T>
    concept CComponent =
        // ComponentRegistry::Add emplaces with no arguments (the "Add Component" path).
        std::is_default_constructible_v<T>
        // entt stores by value and relocates on pool growth.
        && std::is_move_constructible_v<T>
        && requires(nlohmann::json& InJson, const T& InSource, T& InTarget)
        {
            // Requires to_json(json&, const T&) — the json ctor is SFINAE'd on it, so a type
            // without one simply fails to satisfy the concept instead of erroring deep inside
            // a template body.
            { InJson = InSource };
            // Requires from_json(const json&, T&) — get_to is constrained the same way.
            { InJson.get_to(InTarget) };
        };
}
