#pragma once

#include <iterator>
#include <type_traits>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // TEnumValues<E> — the list of E's values. Declare it with OPAAX_ENUM_VALUES.
    //   Used for editor dropdowns and to parse an enum from its label.
    // =============================================================================
    template<typename T>
    struct TEnumValues;

    /**
     * An enum with declared values and a ToString(E).
     */
    template<typename T>
    concept CEnumWithValues = std::is_enum_v<T>
        && requires { TEnumValues<T>::Values; }
        && requires(T InValue) { { ToString(InValue) } -> std::convertible_to<const char*>; };

    /** How many enumerators E declared. */
    template<CEnumWithValues T>
    constexpr Uint64 EnumValueCount() noexcept
    {
        return static_cast<Uint64>(std::size(TEnumValues<T>::Values));
    }
}

// =============================================================================
// Put it right under the enum, at global scope (it opens namespace Opaax), so a new value is not
// forgotten. Name the enum with its namespace:
//
//   namespace Game { enum class EWindowMode { Windowed, Borderless, Fullscreen }; }
//   OPAAX_ENUM_VALUES(Game::EWindowMode, Windowed, Borderless, Fullscreen)
//
// Values are unqualified (the macro uses `using enum`).
// =============================================================================
#define OPAAX_ENUM_VALUES(EnumType, ...)                                        \
    namespace Opaax                                                             \
    {                                                                           \
        template<> struct TEnumValues<EnumType>                                 \
        {                                                                       \
            using enum EnumType;                                                \
            static constexpr EnumType Values[] = { __VA_ARGS__ };               \
        };                                                                      \
    }
