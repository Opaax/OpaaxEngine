#pragma once

#include "Core/Color/LinearColor.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // CEnumWithValues
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Engine/Reflection/PropertyVisitor.h"            // EnumLabels
#include "Engine/Subsystems/Resources/ResourcePath.h"     // TResourcePath
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetRef.h"   // TDataAssetRef
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
    /**
     * A resource reference as a drop target, for any resource type. Shared by the typed drawer below
     * and by the type-erased one.
     * @param InResourceTypeId Only files of this resource type are accepted
     */
    void DrawResourcePathField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               Uint32 InResourceTypeId);

    /**
     * A data asset reference as a drop target: only a .opaaxdata holding InDataType is accepted.
     */
    void DrawDataAssetRefField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               OpaaxStringID InDataType);

    template<CDataAsset T>
    struct TPropertyDrawer<TDataAssetRef<T>>
    {
        static void Draw(IEditorWidgets& InWidgets, const char* InLabel, TDataAssetRef<T>& InValue, const PropertyMeta&)
        {
            DrawDataAssetRefField(InWidgets, InLabel, InValue.Path, DataAssetTypeName<T>());
        }
    };

    /**
     * An enum as a dropdown over its labels. Shared by the typed drawer below and the type-erased one.
     * @param InOutIndex The selected label; written when the user picks another
     */
    void DrawEnumIndexField(IEditorWidgets& InWidgets, const char* InLabel, const char* const* InLabels,
                            Uint32 InCount, Uint32& InOutIndex);

    // One specialization for both load policies (soft or hard only changes what the loader does).
    template<typename TResource, EResourceLoad TLoad>
    struct TPropertyDrawer<TResourcePath<TResource, TLoad>>
    {
        static void Draw(IEditorWidgets& InWidgets, const char* InLabel,
                         TResourcePath<TResource, TLoad>& InValue, const PropertyMeta&)
        {
            DrawResourcePathField(InWidgets, InLabel, InValue.Path, ResourceTypeID::Get<TResource>());
        }
    };

    template<CEnumWithValues T>
    struct TPropertyDrawer<T>
    {
        static void Draw(IEditorWidgets& InWidgets, const char* InLabel, T& InValue, const PropertyMeta&)
        {
            constexpr Uint32 lCount = static_cast<Uint32>(EnumValueCount<T>());

            Uint32 lIndex = 0;
            for (Uint32 i = 0; i < lCount; ++i)
            {
                if (TEnumValues<T>::Values[i] == InValue) { lIndex = i; break; }
            }

            DrawEnumIndexField(InWidgets, InLabel, PropertyVisitorDetail::EnumLabels<T>(), lCount, lIndex);
            InValue = TEnumValues<T>::Values[lIndex];
        }
    };
}
