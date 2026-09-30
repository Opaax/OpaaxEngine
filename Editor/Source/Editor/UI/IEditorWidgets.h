#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorWidgets — the value-editor widgets a property drawer needs to show a field.
    //   Kept backend-neutral because TPropertyDrawer<T> is how games add field types. The list is
    //   closed and every entry has an equivalent in any toolkit. Hover tooltips are passed into the
    //   calls, so nothing depends on submission order.
    // =============================================================================
    class IEditorWidgets
    {
        // =============================================================================
        // Dtor
        // =============================================================================
    public:
        virtual ~IEditorWidgets() = default;

        // =============================================================================
        // Identity
        //   A widget's identity is its label, so two things drawn in one window can collide on a field
        //   name. A scope separates them.
    public:
        virtual void PushId(const char* InId) = 0;

        /** The integer form, for a scope keyed by an id (a loop over tags). */
        virtual void PushId(Uint32 InId) = 0;

        virtual void PopId() = 0;

        // =============================================================================
        // Layout
    public:
        /** Keeps the next item on the current row. */
        virtual void SameLine() = 0;

        /** A horizontal rule. */
        virtual void Separator() = 0;

        /** A vertical separator between groups on a toolbar. */
        virtual void ToolbarSeparator() = 0;

        /** Greys out and disables everything until EndDisabled. */
        virtual void BeginDisabled(bool bInDisabled) = 0;
        virtual void EndDisabled() = 0;

        // =============================================================================
        // Grouping
    public:
        /** A nested group, open by default. @return True when the body must be drawn. */
        virtual bool BeginTreeNode(const char* InLabel) = 0;

        /** Only when BeginTreeNode returned true. */
        virtual void EndTreeNode() = 0;

        /** A collapsible section header, open by default. @return True when the body must be drawn. */
        virtual bool CollapsingHeader(const char* InLabel) = 0;

        // =============================================================================
        // Text
    public:
        virtual void Text(const char* InText) = 0;

        /** Dimmed text: a hint, a unit, a placeholder. */
        virtual void TextDisabled(const char* InText) = 0;

        /** A labelled read-only value. */
        virtual void LabelText(const char* InLabel, const char* InValue) = 0;

        /**
         * A small "(?)" showing InText on hover (a property's tooltip). Its own item, so readers can see an
         * explanation exists.
         */
        virtual void HelpMarker(const char* InText) = 0;

        // =============================================================================
        // Buttons
    public:
        /**
         * @param InWidth 0 fits the label, negative fills the available width
         * @param InTooltip Shown on hover, or null
         */
        virtual bool Button(const char* InLabel, float InWidth = 0.f, const char* InTooltip = nullptr) = 0;

        virtual bool SmallButton(const char* InLabel) = 0;

        // =============================================================================
        // Values: each writes through the reference and returns whether it changed this frame.
    public:
        virtual bool Checkbox(const char* InLabel, bool& InValue) = 0;

        /**
         * One to four floats under one label (a scalar, Vector2F, Vector3F or Vector4F).
         * An unset range (min == max) means unbounded.
         */
        virtual bool DragFloat(const char* InLabel, float* InValues, Uint32 InCount, float InStep,
                               float InMin, float InMax) = 0;

        /**
         * Integer drags, one per type: going through a wider type would let a drag wrap past the real
         * type's bounds.
         */
        virtual bool DragInt16(const char* InLabel, Int16& InValue, Int16 InMin, Int16 InMax) = 0;
        virtual bool DragInt32(const char* InLabel, Int32& InValue, Int32 InMin, Int32 InMax) = 0;
        virtual bool DragUint32(const char* InLabel, Uint32& InValue, Uint32 InMin, Uint32 InMax) = 0;

        /** RGBA, in that order. */
        virtual bool ColorEdit(const char* InLabel, float* InRgba) = 0;

        /**
         * Edits InBuffer in place, NUL-terminated, never writing past InSize.
         * @param bInSubmitOnEnter True to report a change only on Enter
         */
        virtual bool InputText(const char* InLabel, char* InBuffer, Uint32 InSize,
                               bool bInSubmitOnEnter = false) = 0;

        /**
         * The same for a block of text: Enter inserts a line break, and the field is InLineCount rows
         * tall. A separate call because toolkits treat it as a separate widget.
         * @param InLineCount Visible rows. The text scrolls past it
         */
        virtual bool InputTextMultiline(const char* InLabel, char* InBuffer, Uint32 InSize,
                                        Uint32 InLineCount) = 0;

        // =============================================================================
        // Choice
    public:
        /** @return True when the list is open and its items must be emitted. */
        virtual bool BeginCombo(const char* InLabel, const char* InPreview) = 0;

        /** One row. @return True on the frame it is picked. */
        virtual bool Selectable(const char* InLabel, bool bInSelected) = 0;

        /** Scrolls the list to the item just emitted (call it for the selected one). */
        virtual void SetDefaultFocus() = 0;

        /** Only when BeginCombo returned true. */
        virtual void EndCombo() = 0;
    };
}
