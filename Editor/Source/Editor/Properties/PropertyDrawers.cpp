#include "Editor/Properties/PropertyDrawers.h"

#include "Core/Memory/Memory.h"

#include "Editor/Properties/TagPicker.h"
#include "Editor/Properties/WidgetPropertyVisitor.h"
#include "Engine/Tags/TagRegistry.h"
#include "Object/ObjectClass.h"
#include "Resources/Asset/AssetResource.h"

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
        Memory::Copy(lBuffer, InValue.CStr(), InValue.GetLength());
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

            Memory::Copy(lBuffer, lText.Data(), lLen);
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
        /** The field's root tag, or none (every tag). */
        OpaaxTag TagRootOf(const PropertyMeta& InMeta)
        {
            return (InMeta.TagRoot != nullptr && OpaaxTag::IsValidTagText(OpaaxStringView(InMeta.TagRoot)))
                       ? OpaaxTag(InMeta.TagRoot) : OpaaxTag();
        }

        /**
         * A combo of the registered tags under InRoot. @return The one picked this frame, or the invalid tag.
         * InExclude: tags not offered (a set's own).
         */
        OpaaxTag DrawTagCombo(IEditorWidgets& InWidgets, const char* InLabel, const char* InPreview, const OpaaxTag InRoot,
                              const OpaaxTag InSelected, bool& OutPickedNone, const OpaaxTagContainer* InExclude = nullptr)
        {
            OutPickedNone = false;
            const TagRegistry* const lTags = GetEditorTagRegistry();
            if (lTags == nullptr || !InWidgets.BeginCombo(InLabel, InPreview))
            {
                return OpaaxTag();
            }

            OpaaxTag lPicked;
            if (InExclude == nullptr && InWidgets.Selectable("(none)", !InSelected.IsValid()))
            {
                OutPickedNone = true;
            }
            for (const TagPickerRow& lRow : TagPickerRows(*lTags, InRoot))
            {
                if (InExclude != nullptr && InExclude->HasTagExact(lRow.Tag))
                {
                    continue;
                }

                // Keyed by the interned id: two rows with the same last name never share a scope.
                InWidgets.PushId(lRow.Tag.GetName().GetId());
                const OpaaxString lText = lRow.Comment.IsEmpty() ? lRow.Label : lRow.Label + "   - " + lRow.Comment;
                if (InWidgets.Selectable(lText.CStr(), lRow.Tag == InSelected))
                {
                    lPicked = lRow.Tag;
                }
                if (lRow.Tag == InSelected) { InWidgets.SetDefaultFocus(); }
                InWidgets.PopId();
            }
            InWidgets.EndCombo();
            return lPicked;
        }

        /** Under a tag that is not registered: a warning, and a button that lists it in Tags.config. */
        void DrawUnregistered(IEditorWidgets& InWidgets, const OpaaxTag InTag)
        {
            const TagRegistry* const lTags = GetEditorTagRegistry();
            if (lTags == nullptr || !IsTagUnregistered(*lTags, InTag))
            {
                return;
            }

            InWidgets.PushId(InTag.GetName().GetId());
            InWidgets.TextDisabled(OpaaxString::Format("'{}' is not a registered tag", InTag).CStr());
            InWidgets.SameLine();
            if (InWidgets.SmallButton("Register in Tags.config"))
            {
                RegisterTagInConfig(InTag);
            }
            InWidgets.PopId();
        }
    }

    void TPropertyDrawer<OpaaxTag>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                         OpaaxTag& InValue, const PropertyMeta& InMeta)
    {
        InWidgets.PushId(InLabel);

        // The registered tags under the field's root.
        const OpaaxString lPreview = InValue.IsValid() ? InValue.ToString() : OpaaxString("(none)");
        bool             bNone     = false;
        const OpaaxTag   lPicked   = DrawTagCombo(InWidgets, InLabel, lPreview.CStr(), TagRootOf(InMeta), InValue, bNone);
        if (lPicked.IsValid()) { InValue = lPicked; }
        if (bNone)             { InValue = OpaaxTag(); }

        // Or typed, for a tag not registered yet.
        constexpr Uint32 k_BufferSize = 128;

        char lBuffer[k_BufferSize] = {};

        const OpaaxStringView lText = InValue.GetView();
        const Uint32          lLen  = lText.GetLength() < k_BufferSize - 1 ? lText.GetLength() : k_BufferSize - 1;
        Memory::Copy(lBuffer, lText.Data(), lLen);
        lBuffer[lLen] = '\0';

        const bool            bSubmitted = InWidgets.InputText("Typed", lBuffer, k_BufferSize, /*bInSubmitOnEnter*/ true);
        const OpaaxStringView lTyped(lBuffer);
        const bool            bIsTag     = OpaaxTag::IsValidTagText(lTyped);

        // An empty field clears the tag; text that is not a tag is refused (the tag constructor would assert).
        if (bSubmitted && (bIsTag || lTyped.IsEmpty()))
        {
            InValue = bIsTag ? OpaaxTag(lTyped) : OpaaxTag();
        }

        if (!bIsTag && !lTyped.IsEmpty())
        {
            InWidgets.TextDisabled("Not a tag: dotted names, e.g. UI.Layer.Menu");
        }

        DrawUnregistered(InWidgets, InValue);
        InWidgets.PopId();
    }

    void TPropertyDrawer<OpaaxTagContainer>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                                  OpaaxTagContainer& InValue, const PropertyMeta& InMeta)
    {
        InWidgets.PushId(InLabel);

        const bool bOpen = InWidgets.BeginTreeNode(InLabel);
        InWidgets.SameLine();
        InWidgets.TextDisabled(OpaaxString::Format("({})", InValue.Num()).CStr());
        DrawPropertyNote(InWidgets, InMeta);

        if (bOpen)
        {
            // Removed after the loop: the tags are drawn from the set being changed.
            OpaaxTag lRemove;

            for (const OpaaxTag lTag : InValue)
            {
                // Keyed by the interned id: two tags of one set never share a scope.
                InWidgets.PushId(lTag.GetName().GetId());

                if (InWidgets.SmallButton("x")) { lRemove = lTag; }
                InWidgets.SameLine();
                InWidgets.Text(lTag.GetName().CStr());
                DrawUnregistered(InWidgets, lTag);

                InWidgets.PopId();
            }

            if (InValue.IsEmpty())
            {
                InWidgets.TextDisabled("No tags");
            }

            if (lRemove.IsValid()) { InValue.RemoveTag(lRemove); }

            // A registered tag under the field's root, not in the set yet.
            bool           bNone   = false;
            const OpaaxTag lPicked = DrawTagCombo(InWidgets, "Add registered", "(pick a tag)", TagRootOf(InMeta),
                                                  OpaaxTag(), bNone, &InValue);
            if (lPicked.IsValid()) { InValue.AddTag(lPicked); }

            // Or typed. Submit on Enter: the field keeps its own text until then, so a stack buffer is enough.
            constexpr Uint32 k_BufferSize = 128;
            char lBuffer[k_BufferSize] = {};

            if (InWidgets.InputText("Add typed", lBuffer, k_BufferSize, /*bInSubmitOnEnter*/ true))
            {
                // Typed text is untrusted: a tag only when it is one (the tag constructor would assert).
                const OpaaxStringView lTyped(lBuffer);
                if (OpaaxTag::IsValidTagText(lTyped)) { InValue.AddTag(OpaaxTag(lTyped)); }
            }
            InWidgets.TextDisabled("Enter adds a tag: dotted names, e.g. Faction.Player");

            InWidgets.EndTreeNode();
        }

        InWidgets.PopId();
    }

    namespace
    {
        /** A path field as a drop target, for files of the resource type and of the kind InKind. */
        void DrawDropPathField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                               const Uint32 InResourceTypeId, const Uint32 InKind, const char* InEmptyText)
        {
            InWidgets.PushId(InLabel);

            // A button, which is the drop target. The full path is its tooltip (the label truncates).
            const bool  bEmpty = InPath.IsEmpty();
            const char* lText  = bEmpty ? InEmptyText : InPath.CStr();

            InWidgets.Button(lText, -1.f, bEmpty ? nullptr : InPath.CStr());

            // Right after the receiving widget; opens and closes the drop target itself.
            if (OpaaxString lDropped; AcceptResourceDragPayload(InResourceTypeId, lDropped, InKind))
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

    void DrawAssetRefField(IEditorWidgets& InWidgets, const char* InLabel, OpaaxString& InPath,
                           const ObjectClass& InClass)
    {
        const OpaaxString lEmpty = OpaaxString::Format("(drop a {} here)", InClass.GetName().CStr());

        // The payload lists the dropped asset's class and its parents: a subclass matches too.
        DrawDropPathField(InWidgets, InLabel, InPath, ResourceTypeID::Get<AssetResource>(),
                          InClass.GetName().GetId(), lEmpty.CStr());
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

    void DrawPropertyListField(IEditorWidgets& InWidgets, const char* InLabel, IPropertyList& InList,
                               const PropertyMeta& InMeta)
    {
        InWidgets.PushId(InLabel);

        const bool bOpen = InWidgets.BeginTreeNode(InLabel);
        InWidgets.SameLine();
        InWidgets.TextDisabled(OpaaxString::Format("({})", InList.Count()).CStr());
        DrawPropertyNote(InWidgets, InMeta);

        if (bOpen)
        {
            // Applied after the loop: the elements are drawn from the list being changed.
            Int64 lRemove = -1;
            Int64 lMoveUp = -1;

            // The elements keep the list's range and step; its note is shown once, beside its name.
            PropertyMeta lElementMeta = InMeta;
            lElementMeta.Tooltip = nullptr;
            lElementMeta.Flags   = EPropertyFlags::None;

            WidgetPropertyVisitor lVisitor(InWidgets);
            for (Uint32 lIndex = 0; lIndex < InList.Count(); ++lIndex)
            {
                InWidgets.PushId(lIndex);

                if (InWidgets.SmallButton("x"))
                {
                    lRemove = lIndex;
                }
                InWidgets.SameLine();

                InWidgets.BeginDisabled(lIndex == 0);
                if (InWidgets.SmallButton("^"))
                {
                    lMoveUp = lIndex;
                }
                InWidgets.EndDisabled();
                InWidgets.SameLine();

                InList.VisitElement(lIndex, PropertyListLabel(lIndex).Text, lVisitor, lElementMeta);
                InWidgets.PopId();
            }

            if (InWidgets.SmallButton("+ Add"))
            {
                InList.Add();
            }

            if (lRemove >= 0)      { InList.RemoveAt(static_cast<Uint32>(lRemove)); }
            else if (lMoveUp > 0)  { InList.Swap(static_cast<Uint32>(lMoveUp - 1), static_cast<Uint32>(lMoveUp)); }

            InWidgets.EndTreeNode();
        }

        InWidgets.PopId();
    }
}
