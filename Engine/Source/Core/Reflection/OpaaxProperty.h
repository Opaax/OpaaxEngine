#pragma once

#include <tuple>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // Property list — the editable fields of a type (component, config, ...), as data.
    //   The editor picks a widget per field type. An unsupported field type is a compile error.
    //   Costs nothing when unused.
    // =============================================================================

    /**
     * Property flags (bitmask).
     */
    enum class EPropertyFlags : Uint8
    {
        None        = 0,
        /** Changes apply at the next launch (the value is read at startup). */
        NeedRestart = BIT(0),

        /**
         * Multi-line text.
         */
        Multiline   = BIT(1),
    };

    constexpr EPropertyFlags operator|(const EPropertyFlags InA, const EPropertyFlags InB) noexcept
    {
        return static_cast<EPropertyFlags>(static_cast<Uint8>(InA) | static_cast<Uint8>(InB));
    }

    constexpr bool HasFlag(const EPropertyFlags InValue, const EPropertyFlags InFlag) noexcept
    {
        return (static_cast<Uint8>(InValue) & static_cast<Uint8>(InFlag)) != 0;
    }

    /**
     * Extra info about a field: range, flags and tooltip.
     * Min == Max means no range.
     */
    struct PropertyMeta
    {
        float          RangeMin = 0.f;
        float          RangeMax = 0.f;
        float          DragStep = 0.f;
        EPropertyFlags Flags    = EPropertyFlags::None;
        const char*    Tooltip  = nullptr;
    };

    // =============================================================================
    // TProperty — one field: its name and member pointer.
    // =============================================================================
    template<typename TClass, typename TValue>
    struct TProperty
    {
        using ClassType = TClass;
        using ValueType = TValue;

        const char*      Name   = nullptr;
        TValue TClass::* Member = nullptr;
        PropertyMeta     Meta;

        /**
         * Clamps the value between InMin and InMax. Returns a modified copy.
         */
        constexpr TProperty SetRange(const float InMin, const float InMax) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.RangeMin = InMin;
            lCopy.Meta.RangeMax = InMax;

            return lCopy;
        }
        
        constexpr TProperty SetDragStep(const float InStep) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.DragStep = InStep;

            return lCopy;
        }

        /**
         * Sets the flags. Can be set on a group (a nested reflected type).
         */
        constexpr TProperty SetFlags(const EPropertyFlags InFlags) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.Flags = InFlags;

            return lCopy;
        }

        /**
         * Tooltip text. Pass a literal.
         */
        constexpr TProperty SetTooltip(const char* InText) const noexcept
        {
            TProperty lCopy = *this;
            lCopy.Meta.Tooltip = InText;

            return lCopy;
        }
    };

    template<typename TClass, typename TValue>
    constexpr TProperty<TClass, TValue> MakeProperty(const char* InName, TValue TClass::* InMember) noexcept
    {
        return TProperty<TClass, TValue>{InName, InMember};
    }

    /**
     * InMeta with its unset range and drag step taken from the group above (e.g. a UIMargin is pixels
     * in one place and a fraction in another). A field's own range still wins.
     */
    constexpr PropertyMeta InheritMeta(const PropertyMeta& InMeta, const PropertyMeta& InInherited) noexcept
    {
        PropertyMeta lMeta = InMeta;

        if (lMeta.RangeMin == lMeta.RangeMax)
        {
            lMeta.RangeMin = InInherited.RangeMin;
            lMeta.RangeMax = InInherited.RangeMax;
        }
        if (lMeta.DragStep == 0.f)
        {
            lMeta.DragStep = InInherited.DragStep;
        }

        return lMeta;
    }

    /**
     * A type that lists its properties (OPAAX_PROPERTIES). Optional.
     */
    template<typename T>
    concept CReflected = requires { T::GetProperties(); };
}

// =============================================================================
// Lists the fields the editor can edit. Put it next to
// NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT:
//
//   OPAAX_PROPERTIES(DummyComponent,
//       OPAAX_PROP(Position),
//       OPAAX_PROP(Size).SetRange(1.f, 4096.f))
// =============================================================================
#define OPAAX_PROPERTIES(ClassName, ...)                                            \
    using PropertyOwnerType = ClassName;                                            \
    static constexpr auto GetProperties() noexcept                                  \
    { return ::std::make_tuple(__VA_ARGS__); }

#define OPAAX_PROP(Field) ::Opaax::MakeProperty(#Field, &PropertyOwnerType::Field)
