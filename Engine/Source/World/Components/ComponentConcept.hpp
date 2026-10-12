#pragma once

#include "Core/Templates/TypeTraits.h"

#include <nlohmann/json.hpp>

#include "Core/Serialization/JsonConcept.h"

// =============================================================================
// CComponent — the compile-time contract for a serializable component (no base class,
//   since entt stores components by value).
//
//   A component needs nlohmann's to_json/from_json. One macro gives both:
//       NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(T, fields...)
//   Use the _WITH_DEFAULT variant: the plain one throws on a missing key, so adding a field
//   would break every saved map. MapFactory reports what defaults cannot cover.
// =============================================================================
namespace Opaax
{
    template<typename T>
    concept CComponent =
        // Default-constructible (the "Add Component" path).
        TIsDefaultConstructible<T>
        // Movable: entt relocates components when its pool grows.
        && TIsMoveConstructible<T>
        // The JSON part, shared with config data types (Core/Serialization/JsonConcept.h).
        && CJsonSerializable<T>;
}
