#pragma once

#include <tuple>

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax::Editor
{
    // =============================================================================
    // TPropertyDrawer<T> — the widget for a field of type T. DECLARED, NEVER DEFINED.
    //
    //   The customization point is a SPECIALIZATION, not a registry entry, and the primary template
    //   is left undefined on purpose — the same trade TConfigCodec makes, for the same stated
    //   reason: "you forgot to specialize" becomes a compile error naming the type instead of a
    //   silent fallback. That is what makes visibility safe here. A drawer seen by one TU and not
    //   another cannot instantiate the fold two different ways (a silent ODR break); the second TU
    //   simply fails to build.
    //
    //   A registry would only buy registering a drawer for a type you cannot include — which nobody
    //   needs — and would cost a lookup per field per frame plus something to seal.
    //
    //   The contract: static void Draw(const char* InLabel, T& InValue, EPropertyHint InHint).
    //   Uniform, so the fold needs no dispatch of its own; most types ignore the hint.
    // =============================================================================
    template<typename T>
    struct TPropertyDrawer;

    /**
     * Draw one described field of InOwner.
     *
     * The member pointer carries the field's type, so this resolves the right specialization with
     * no runtime lookup and no type tag.
     */
    template<typename TProperty, typename TOwner>
    void DrawProperty(const TProperty& InProperty, TOwner& InOwner)
    {
        using ValueType = typename TProperty::ValueType;

        TPropertyDrawer<ValueType>::Draw(InProperty.Name, InOwner.*(InProperty.Member), InProperty.Hint);
    }

    /**
     * Draw every property InOwner describes, in declaration order.
     *
     * Writes STRAIGHT INTO the live object, as a hand-written drawer does — the Inspector marks the
     * world changed centrally off ImGui::IsAnyItemActive(), so nothing here has to report an edit.
     */
    template<CReflected TOwner>
    void DrawProperties(TOwner& InOwner)
    {
        std::apply([&InOwner](const auto&... lProperties)
                   {
                       (DrawProperty(lProperties, InOwner), ...);
                   },
                   TOwner::GetProperties());
    }

    /** How many properties T describes — known at compile time, so it can be logged at registration. */
    template<CReflected T>
    constexpr Uint64 PropertyCount() noexcept
    {
        return static_cast<Uint64>(std::tuple_size_v<decltype(T::GetProperties())>);
    }
}
