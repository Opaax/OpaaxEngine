#pragma once

#include <tuple>

#include "Editor/UI/IEditorWidgets.h"

#include "Core/Reflection/OpaaxProperty.h"

namespace Opaax::Editor
{
    // =============================================================================
    // TPropertyDrawer<T> — the widget for a field of type T. Declared, never defined: a missing
    //   specialization is a compile error naming the type, not a silent fallback.
    //   Contract: static void Draw(IEditorWidgets&, const char* InLabel, T& InValue,
    //   const PropertyMeta& InMeta). Games specialize it to support new field types, so drawers use
    //   IEditorWidgets and never a backend directly.
    // =============================================================================
    template<typename T>
    struct TPropertyDrawer;

    /**
     * Draws what the meta adds beside the value: the field's tooltip, then its flags (e.g. a
     * NeedRestart field says the change applies on next launch). Done here so every drawer gets it.
     */
    inline void DrawPropertyNote(IEditorWidgets& InWidgets, const PropertyMeta& InMeta)
    {
        if (InMeta.Tooltip != nullptr)
        {
            InWidgets.SameLine();
            InWidgets.HelpMarker(InMeta.Tooltip);
        }

        if (HasFlag(InMeta.Flags, EPropertyFlags::NeedRestart))
        {
            InWidgets.SameLine();
            InWidgets.TextDisabled("(restart)");
        }
    }

    // Declared ahead of DrawProperty: the two call each other (a group is a property whose value has
    // properties).
    template<CReflected TOwner>
    void DrawProperties(IEditorWidgets& InWidgets, TOwner& InOwner, const PropertyMeta& InInherited = {});

    /**
     * Draws one value by hand, outside a property list (for custom drawers laying out their own
     * groups). Uses the same TPropertyDrawer<T> as the generic fold. The label is the caller's.
     */
    template<typename TValue>
    void DrawField(IEditorWidgets& InWidgets, const char* InLabel, TValue& InValue,
                   const PropertyMeta& InMeta = {})
    {
        TPropertyDrawer<TValue>::Draw(InWidgets, InLabel, InValue, InMeta);
    }

    /**
     * Draws one described field of InOwner. The member pointer carries the field's type, so the right
     * specialization is picked at compile time.
     */
    template<typename TProperty, typename TOwner>
    void DrawProperty(IEditorWidgets& InWidgets, const TProperty& InProperty, TOwner& InOwner,
                      const PropertyMeta& InInherited = {})
    {
        using ValueType = typename TProperty::ValueType;

        // Value facets from the field or the group above; the note stays the field's own.
        const PropertyMeta lMeta = InheritMeta(InProperty.Meta, InInherited);

        // A field that describes its own fields is a group, not a widget (lets configs nest).
        if constexpr (CReflected<ValueType>)
        {
            if (InWidgets.BeginTreeNode(InProperty.Name))
            {
                DrawPropertyNote(InWidgets, InProperty.Meta);
                DrawProperties(InWidgets, InOwner.*(InProperty.Member), lMeta);
                InWidgets.EndTreeNode();
            }
        }
        else
        {
            TPropertyDrawer<ValueType>::Draw(InWidgets, InProperty.Name, InOwner.*(InProperty.Member), lMeta);
            DrawPropertyNote(InWidgets, InProperty.Meta);
        }
    }

    /**
     * Draws every property InOwner describes, in declaration order. Writes directly into the object;
     * the Inspector detects edits itself.
     */
    template<CReflected TOwner>
    void DrawProperties(IEditorWidgets& InWidgets, TOwner& InOwner, const PropertyMeta& InInherited)
    {
        std::apply([&InWidgets, &InOwner, &InInherited](const auto&... lProperties)
                   {
                       (DrawProperty(InWidgets, lProperties, InOwner, InInherited), ...);
                   },
                   TOwner::GetProperties());
    }

    /** How many properties T describes (known at compile time). */
    template<CReflected T>
    constexpr Uint64 PropertyCount() noexcept
    {
        return static_cast<Uint64>(std::tuple_size_v<decltype(T::GetProperties())>);
    }
}
