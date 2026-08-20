#pragma once

#include <iterator>      // std::size
#include <type_traits>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // TEnumValues<E> — the enumerators of E, as data. DECLARED, NEVER DEFINED.
    //
    //   C++20 cannot enumerate an enum, so the list has to be written once, and this is the third
    //   place the engine uses the same customization point shape (TConfigCodec, TPropertyDrawer):
    //   an undefined primary makes "you never declared the values" a compile error naming the enum,
    //   rather than a dropdown that silently opens onto nothing.
    //
    //   The values are the PARSER too. With a list and I11's free ToString(E), reading a label is a
    //   scan — which is why no enum needs a FromString of its own any more, and why the json bridge
    //   (OpaaxEnumJson.h) is one template instead of one function per enum.
    // =============================================================================
    template<typename T>
    struct TEnumValues;

    /**
     * An enum that declared its values AND can name them.
     *
     * The ToString requirement is resolved by ADL at the point of use, which is what lets the
     * mapping live beside each enum (I11) instead of in a table this header would have to know.
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
// Stamp DIRECTLY UNDER the enum it describes — that placement is the only defence against a
// forgotten enumerator, since nothing in C++20 can check the list is complete:
//
//   enum class EWindowMode { Windowed, Borderless, Fullscreen };
//   OPAAX_ENUM_VALUES(EWindowMode, Windowed, Borderless, Fullscreen)
//
// The enumerators are UNQUALIFIED because the specialization opens with `using enum` (C++20 P1099),
// so the list reads like the enum above it rather than repeating the type name per entry.
// =============================================================================
#define OPAAX_ENUM_VALUES(EnumType, ...)                                        \
    template<> struct ::Opaax::TEnumValues<EnumType>                            \
    {                                                                           \
        using enum EnumType;                                                    \
        static constexpr EnumType Values[] = { __VA_ARGS__ };                   \
    };
