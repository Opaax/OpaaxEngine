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
    /**
     * Never moved by entt once built (in_place_delete: removing another leaves a hole instead of moving
     * the last one in). A behaviour is one: an object, which cannot be moved.
     */
    template<typename T>
    concept CPinnedComponent = requires { requires T::in_place_delete; };

    template<typename T>
    concept CComponent =
        // Default-constructible (the "Add Component" path).
        TIsDefaultConstructible<T>
        // Movable: entt moves the last component into the slot of one removed. A pinned one stays put.
        && (TIsMoveConstructible<T> || CPinnedComponent<T>)
        // The JSON part, shared with config data types (Core/Serialization/JsonConcept.h).
        && CJsonSerializable<T>;
}
