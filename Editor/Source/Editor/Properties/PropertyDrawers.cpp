#include "Editor/Properties/PropertyDrawers.h"

#include <cstring>   // memcpy
#include <string>

#include "Engine/Subsystems/Resources/Types/DataAsset/DataAssetResource.h"

#include <glm/gtc/type_ptr.hpp>   // value_ptr

namespace Opaax::Editor
{
    // Every drawer uses IEditorWidgets only (no backend). An unset range (min == max) means unbounded.

    void TPropertyDrawer<bool>::Draw(IEditorWidgets& InWidgets, const char* InLabel, bool& InValue,
                                     const PropertyMeta&)
    {
        InWidgets.Checkbox(InLabel, InValue);
    }

    void TPropertyDrawer<Int16>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Int16& InValue,
                                      const PropertyMeta& InMeta)
    {
        // Clamped at the real type's bounds (a wider type would wrap past 32767).
        const bool  lHasRange = InMeta.RangeMax > InMeta.RangeMin;
        const Int16 lMin      = lHasRange ? static_cast<Int16>(InMeta.RangeMin) : Int16{-32768};
        const Int16 lMax      = lHasRange ? static_cast<Int16>(InMeta.RangeMax) : Int16{32767};

        InWidgets.DragInt16(InLabel, InValue, lMin, lMax);
    }

    void TPropertyDrawer<Int32>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Int32& InValue,
                                      const PropertyMeta& InMeta)
    {
        InWidgets.DragInt32(InLabel, InValue,
                            static_cast<Int32>(InMeta.RangeMin), static_cast<Int32>(InMeta.RangeMax));
    }

    void TPropertyDrawer<Uint32>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Uint32& InValue,
                                       const PropertyMeta& InMeta)
    {
        // Zero is the floor, range or not (a wider type would wrap to ~4 billion).
        const Uint32 lMin = InMeta.RangeMin > 0.f ? static_cast<Uint32>(InMeta.RangeMin) : 0u;
        const Uint32 lMax = InMeta.RangeMax > InMeta.RangeMin ? static_cast<Uint32>(InMeta.RangeMax) : 0u;

        InWidgets.DragUint32(InLabel, InValue, lMin, lMax);
    }

    void TPropertyDrawer<float>::Draw(IEditorWidgets& InWidgets, const char* InLabel, float& InValue,
                                      const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, &InValue, 1,InMeta.DragStep, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector2F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector2F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 2,InMeta.DragStep, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector3F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector3F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 3,InMeta.DragStep, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector4F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector4F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 4,InMeta.DragStep, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<LinearColor>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                            LinearColor& InValue, const PropertyMeta&)
    {
        InWidgets.ColorEdit(InLabel, glm::value_ptr(static_cast<Vector4F&>(InValue)));
    }

    void TPropertyDrawer<OpaaxString>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                            OpaaxString& InValue, const PropertyMeta& InMeta)
    {
        // A stack buffer, filled from the value every frame (the value stays the source of truth).
        // Multi-line text gets a bigger one.
        constexpr Uint32 k_LineSize   = 512;
        constexpr Uint32 k_BlockSize  = 4096;
        constexpr Uint32 k_BlockLines = 5;

        const bool   bMultiline = HasFlag(InMeta.Flags, EPropertyFlags::Multiline);
        const Uint32 lCapacity  = bMultiline ? k_BlockSize : k_LineSize;

        // Refused rather than truncated: a value this long is not editable here, and says so.
        if (InValue.GetLength() >= lCapacity)
        {
            InWidgets.LabelText(InLabel, InValue.CStr());
            InWidgets.SameLine();
            InWidgets.TextDisabled("(too long to edit)");
            return;
        }

        char lBuffer[k_BlockSize];
        std::memcpy(lBuffer, InValue.CStr(), InValue.GetLength());
        lBuffer[InValue.GetLength()] = '\0';

        const bool bEdited = bMultiline
                                 ? InWidgets.InputTextMultiline(InLabel, lBuffer, lCapacity, k_BlockLines)
                                 : InWidgets.InputText(InLabel, lBuffer, lCapacity);

        if (bEdited)
        {
            InValue = OpaaxString(lBuffer);
        }
    }

    void TPropertyDrawer<OpaaxStringID>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                              OpaaxStringID& InValue, const PropertyMeta&)
    {
        // Submit on Enter: ids are interned and never freed, so interning every keystroke would leak.
        constexpr Uint32 k_BufferSize = 128;

        char lBuffer[k_BufferSize] = {};

        // IsValid, not ToString: an invalid id reads "None", which would become a real name on Enter.
        if (InValue.IsValid())
        {
            const OpaaxStringView lText = InValue.GetView();
            const Uint32          lLen  = lText.GetLength() < k_BufferSize - 1
                                              ? lText.GetLength() : k_BufferSize - 1;

            std::memcpy(lBuffer, lText.Data(), lLen);
            lBuffer[lLen] = '\0';
        }

        if (InWidgets.InputText(InLabel, lBuffer, k_BufferSize, /*bInSubmitOnEnter*/ true))
        {
            // An empty field means the invalid id (unnamed), not an interned empty string.
            InValue = (lBuffer[0] == '\0') ? OpaaxStringID() : OpaaxStringID(OpaaxString(lBuffer));
        }
    }

    namespace
    {
        /** A path field as a drop target. InSubTypeId 0 accepts any file of the resource type. */
        void DrawDropPathField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               const Uint32 InResourceTypeId, const Uint32 InSubTypeId, const char* InEmptyText)
        {
            InWidgets.PushId(InLabel);

            // A button, which is the drop target. The full path is its tooltip (the label truncates).
            const bool  bEmpty = InPath.IsEmpty();
            const char* lText  = bEmpty ? InEmptyText : InPath.CStr();

            InWidgets.Button(lText, -1.f, bEmpty ? nullptr : InPath.CStr());

            // Right after the receiving widget; opens and closes the drop target itself.
            if (OpaaxString lDropped; AcceptResourceDragPayload(InResourceTypeId, lDropped, InSubTypeId))
            {
                InPath = Move(lDropped);
            }

            if (!InPath.IsEmpty())
            {
                InWidgets.SameLine();
                if (InWidgets.SmallButton("x")) { InPath = OpaaxString(); }
            }

            InWidgets.SameLine();
            InWidgets.Text(InLabel);

            InWidgets.PopId();
        }
    }

    void DrawDataAssetRefField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               const OpaaxStringID InDataType)
    {
        const std::string lEmpty = std::string("(drop a ") + (InDataType.IsValid() ? InDataType.CStr() : "data asset")
                                 + " here)";

        DrawDropPathField(InWidgets, InLabel, InPath, ResourceTypeID::Get<DataAssetResource>(), InDataType.GetId(),
                          lEmpty.c_str());
    }

    void DrawResourcePathField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               const Uint32 InResourceTypeId)
    {
        DrawDropPathField(InWidgets, InLabel, InPath, InResourceTypeId, 0, "(drop a resource here)");
    }

    void DrawEnumIndexField(IEditorWidgets& InWidgets, const char* InLabel, const char* const* InLabels,
                            const Uint32 InCount, Uint32& InOutIndex)
    {
        if (InCount == 0 || !InWidgets.BeginCombo(InLabel, InLabels[InOutIndex < InCount ? InOutIndex : 0]))
        {
            return;
        }

        for (Uint32 i = 0; i < InCount; ++i)
        {
            const bool bSelected = i == InOutIndex;

            if (InWidgets.Selectable(InLabels[i], bSelected))
            {
                InOutIndex = i;
            }

            if (bSelected)
            {
                InWidgets.SetDefaultFocus();
            }
        }

        InWidgets.EndCombo();
    }
}
