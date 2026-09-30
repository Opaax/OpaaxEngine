#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // CEnumWithValues
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Subsystems/Resources/ResourcePath.h"     // TResourcePath
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp" // ResourceTypeID
#include "Editor/Properties/PropertyDrawer.h"
#include "Editor/Resources/ResourceDragDrop.h"

namespace Opaax::Editor
{
    // =============================================================================
    // The built-in property drawers, for the engine's basic value types. Declarations only (bodies in
    //   the .cpp); all use IEditorWidgets. Dispatch is by type: LinearColor gets a colour picker, a
    //   plain Vector4F gets four drags.
    // =============================================================================

#define OPAAX_DECLARE_PROPERTY_DRAWER(Type)                                                 \
    template<> struct TPropertyDrawer<Type>                                                 \
    { static void Draw(IEditorWidgets& InWidgets, const char* InLabel, Type& InValue,             \
                       const PropertyMeta& InMeta); }

    OPAAX_DECLARE_PROPERTY_DRAWER(bool);
    OPAAX_DECLARE_PROPERTY_DRAWER(Int16);
    OPAAX_DECLARE_PROPERTY_DRAWER(Int32);
    OPAAX_DECLARE_PROPERTY_DRAWER(Uint32);
    OPAAX_DECLARE_PROPERTY_DRAWER(float);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector2F);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector3F);
    OPAAX_DECLARE_PROPERTY_DRAWER(Vector4F);
    OPAAX_DECLARE_PROPERTY_DRAWER(LinearColor);
    OPAAX_DECLARE_PROPERTY_DRAWER(OpaaxString);
    OPAAX_DECLARE_PROPERTY_DRAWER(OpaaxStringID);

#undef OPAAX_DECLARE_PROPERTY_DRAWER

    // =============================================================================
    // Every enum at once: an enum with OPAAX_ENUM_VALUES gets a dropdown, labelled by its ToString.
    //   Inline because it is a template.
    // =============================================================================
    // =============================================================================
    // Every resource reference at once. Filled by dragging a file from the Resource Browser onto it.
    //   A file of another resource type is refused (no highlight, the drag stays live).
    // =============================================================================
    // One specialization for both load policies (soft or hard only changes what the loader does).
    template<typename TResource, EResourceLoad TLoad>
    struct TPropertyDrawer<TResourcePath<TResource, TLoad>>
    {
        static void Draw(IEditorWidgets& InWidgets, const char* InLabel,
                         TResourcePath<TResource, TLoad>& InValue, const PropertyMeta&)
        {
            InWidgets.PushId(InLabel);

            // A button, which is the drop target. The full path is its tooltip (the label truncates).
            const char* lText = InValue.IsEmpty() ? "(drop a resource here)" : InValue.Path.CStr();

            InWidgets.Button(lText, -1.f, InValue.IsEmpty() ? nullptr : InValue.Path.CStr());

            // Right after the receiving widget; opens and closes the drop target itself.
            if (OpaaxString lDropped; AcceptResourceDragPayload(ResourceTypeID::Get<TResource>(), lDropped))
            {
                InValue.Path = Move(lDropped);
            }

            if (!InValue.IsEmpty())
            {
                InWidgets.SameLine();
                if (InWidgets.SmallButton("x")) { InValue.Path = OpaaxString(); }
            }

            InWidgets.SameLine();
            InWidgets.Text(InLabel);

            InWidgets.PopId();
        }
    };

    template<CEnumWithValues T>
    struct TPropertyDrawer<T>
    {
        static void Draw(IEditorWidgets& InWidgets, const char* InLabel, T& InValue, const PropertyMeta&)
        {
            if (!InWidgets.BeginCombo(InLabel, ToString(InValue)))
            {
                return;
            }

            for (const T lCandidate : TEnumValues<T>::Values)
            {
                const bool bSelected = lCandidate == InValue;

                if (InWidgets.Selectable(ToString(lCandidate), bSelected))
                {
                    InValue = lCandidate;
                }

                if (bSelected)
                {
                    InWidgets.SetDefaultFocus();
                }
            }

            InWidgets.EndCombo();
        }
    };
}
