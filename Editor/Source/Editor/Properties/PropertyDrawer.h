#pragma once

#include <tuple>

#include <imgui.h>

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
    //   The contract: static void Draw(const char* InLabel, T& InValue, const PropertyMeta& InMeta).
    //   Uniform, so the fold needs no dispatch of its own — and the meta arrives as ONE object so a
    //   new facet never changes this signature.
    // =============================================================================
    template<typename T>
    struct TPropertyDrawer;

    /**
     * What the value's FLAGS have to say, under the value.
     *
     * Editing a NeedRestart field does nothing visible until the next launch, and a UI that stays
     * silent about that reads as a bug in the field. Stated per property (usually per GROUP), never
     * as a blanket line on the panel — the blanket version goes stale the day one value is read live
     * and nobody remembers to update the sentence.
     */
    inline void DrawPropertyNote(const PropertyMeta& InMeta)
    {
        if (!HasFlag(InMeta.Flags, EPropertyFlags::NeedRestart)) { return; }

        ImGui::SameLine();
        ImGui::TextDisabled("(restart)");
    }

    // Declared ahead of DrawProperty because the two are MUTUALLY RECURSIVE: a group is a property
    // whose value has properties of its own.
    template<CReflected TOwner>
    void DrawProperties(TOwner& InOwner);

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

        // A field that describes ITS OWN fields is a GROUP, not a widget — which is what lets a
        // config nest (Window: {Title, Width…}) with one declaration doing the file, the C++ and the
        // UI. Without this a nested type would demand a TPropertyDrawer specialization that could
        // never sensibly exist.
        if constexpr (CReflected<ValueType>)
        {
            if (ImGui::TreeNodeEx(InProperty.Name, ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawPropertyNote(InProperty.Meta);
                DrawProperties(InOwner.*(InProperty.Member));
                ImGui::TreePop();
            }
        }
        else
        {
            TPropertyDrawer<ValueType>::Draw(InProperty.Name, InOwner.*(InProperty.Member), InProperty.Meta);
            DrawPropertyNote(InProperty.Meta);
        }
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
