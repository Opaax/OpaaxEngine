#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorWidgets — the VALUE-EDITOR vocabulary: what a property drawer needs to put a field on
    //   screen, and nothing else.
    //
    //   ITS OWN SEAM, not a section of IEditorGui, for the reason EditorContext already gives for
    //   IEditorUIBackend and IEditorDialogs: it is the narrower dependency. A drawer that edits a
    //   float has no business with the menu bar, the dockspace or a panel window.
    //
    //   WHY THIS EXISTS WHEN MR2d RULED WIDGETS STAY DIRECT. That ruling was about the ~360 call
    //   sites of bespoke panel UI, and it stands for them. A property drawer is the opposite kind of
    //   thing: `TPropertyDrawer<T>` is the extension point a GAME uses to support a new field type
    //   so leaving it backend-bound binds every game's custom field editor forever. The
    //   vocabulary below is CLOSED and was derived from the call sites that exist — not invented —
    //   and every entry is a value editor any toolkit has a direct equivalent for.
    //
    //   MR2d's specific objection was that a widget API "encodes that backend's shape — immediate
    //   mode, an ID stack, IsItemHovered late-bound to submission order." That is answered rather
    //   than ignored: the late-bound queries are FOLDED INTO the calls (Button takes its tooltip),
    //   so nothing here depends on submission order, and the id scope is explicit rather than
    //   implied.
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
        //
        //   A widget's identity is its LABEL, so two independently-authored things drawn into one
        //   window can collide on a shared field name (I15). The scope is the boundary between them.
    public:
        virtual void PushId(const char* InId) = 0;

        /** The integer form, for a scope keyed by an id rather than a name (a loop over tags). */
        virtual void PushId(Uint32 InId) = 0;

        virtual void PopId() = 0;

        // =============================================================================
        // Layout
    public:
        /** Keep the next item on the current row. */
        virtual void SameLine() = 0;

        /** A horizontal rule. */
        virtual void Separator() = 0;

        /** A vertical rule between groups on a toolbar strip. */
        virtual void ToolbarSeparator() = 0;

        /** Grey out and refuse input for everything until EndDisabled. */
        virtual void BeginDisabled(bool bInDisabled) = 0;
        virtual void EndDisabled() = 0;

        // =============================================================================
        // Grouping
    public:
        /** A nested group, open by default. @return true when the body must be drawn. */
        virtual bool BeginTreeNode(const char* InLabel) = 0;

        /** Only when BeginTreeNode returned true — the backend's pairing rule. */
        virtual void EndTreeNode() = 0;

        /** A collapsible section header, open by default. @return true when the body must be drawn. */
        virtual bool CollapsingHeader(const char* InLabel) = 0;

        // =============================================================================
        // Text
    public:
        virtual void Text(const char* InText) = 0;

        /** Dimmed — a hint, a unit, a "nothing here" placeholder. */
        virtual void TextDisabled(const char* InText) = 0;

        /** A labelled read-only value, for something that cannot be edited in place. */
        virtual void LabelText(const char* InLabel, const char* InValue) = 0;

        // =============================================================================
        // Buttons
    public:
        /**
         * @param InWidth 0 for "fit the label", negative for "fill the available width".
         * @param InTooltip Shown on hover, or null. TAKEN HERE rather than left to a follow-up
         *   hover query: that query would bind this seam to submission order, which is exactly what
         *   MR2d warns a widget API must not do.
         */
        virtual bool Button(const char* InLabel, float InWidth = 0.f, const char* InTooltip = nullptr) = 0;

        virtual bool SmallButton(const char* InLabel) = 0;

        // =============================================================================
        // Values — each writes THROUGH the reference and answers whether it changed this frame.
    public:
        virtual bool Checkbox(const char* InLabel, bool& InValue) = 0;

        /**
         * One to four floats behind one label — a scalar, a Vector2F, a Vector3F or a Vector4F.
         *
         * ONE call with a COUNT rather than four: the widget is the same thing at four widths, and
         * a backend that does it differently would still only implement this once.
         *
         * An unset range arrives as min == max, which every drag reads as unbounded — so the
         * ordinary property needs no flag and no branch here.
         */
        virtual bool DragFloat(const char* InLabel, float* InValues, Uint32 InCount,
                               float InMin, float InMax) = 0;

        /**
         * The integer drags are TYPED, one per width, and that is not verbosity.
         *
         * A round trip through a wider type lets a drag past the real type's bounds WRAP — past
         * 32767 to a large negative draw order, or below zero to ~4 billion — so the clamp has to
         * happen at the type the value actually is.
         */
        virtual bool DragInt16(const char* InLabel, Int16& InValue, Int16 InMin, Int16 InMax) = 0;
        virtual bool DragInt32(const char* InLabel, Int32& InValue, Int32 InMin, Int32 InMax) = 0;
        virtual bool DragUint32(const char* InLabel, Uint32& InValue, Uint32 InMin, Uint32 InMax) = 0;

        /** RGBA, in that order. */
        virtual bool ColorEdit(const char* InLabel, float* InRgba) = 0;

        /**
         * Edits InBuffer in place, NUL-terminated, never writing past InSize.
         *
         * @param bInSubmitOnEnter true to answer only on Enter rather than on every keystroke.
         */
        virtual bool InputText(const char* InLabel, char* InBuffer, Uint32 InSize,
                               bool bInSubmitOnEnter = false) = 0;

        // =============================================================================
        // Choice
    public:
        /** @return true when the list is open and its items must be emitted. */
        virtual bool BeginCombo(const char* InLabel, const char* InPreview) = 0;

        /** One row. @return true on the frame it is picked. */
        virtual bool Selectable(const char* InLabel, bool bInSelected) = 0;

        /** Scroll the list to the item just emitted — call for the selected one. */
        virtual void SetDefaultFocus() = 0;

        /** Only when BeginCombo returned true. */
        virtual void EndCombo() = 0;
    };
}
