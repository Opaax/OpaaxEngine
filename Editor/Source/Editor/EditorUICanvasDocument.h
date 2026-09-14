#pragma once

#include "Application/Services/ILogger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "UI/UICanvas.h"

namespace Opaax
{
    class UIWidget;
    class UIWidgetRegistry;
}

namespace Opaax::Editor
{
    inline constexpr LogCategory LogEditorUICanvasDocument{"EditorUICanvasDocument"};

    /**
     * WHERE a widget sits, as child indices from the root — `{0, 2}` is "the root's first child's
     * third child". The root itself is the empty path.
     *
     * A PATH RATHER THAN A POINTER, and that is the whole reason this type exists: an undo step
     * replaces the tree wholesale (**UI15**), so every raw pointer into it dies. The prefab
     * document solves the same problem with entity guids; a tree has no ids, and its shape IS the
     * address.
     */
    using UIWidgetPath = TDynArray<Uint32>;

    // =============================================================================
    // EditorUICanvasDocument — WHICH `.opaaxui` is open, its live canvas, what is selected, and
    //   whether the tree still matches what was last written.
    //
    //   `EditorFontFamilyDocument`'s shape (Open / Close / MarkSaved / IsDirty-by-reserialize) with
    //   one difference that matters: it owns a real `UICanvas`, not plain data, because the panel
    //   PREVIEWS it by rendering it (**UI14**). What is on screen is the document itself, so a
    //   preview cannot drift from what a Save would write.
    // =============================================================================
    class EditorUICanvasDocument
    {
        // =============================================================================
        // Ctor
        // =============================================================================
    public:
        EditorUICanvasDocument() = default;

        EditorUICanvasDocument(const EditorUICanvasDocument&)            = delete;
        EditorUICanvasDocument& operator=(const EditorUICanvasDocument&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Read InAbsPath and make it the open canvas. A failure leaves the previous one open. */
        bool Open(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry);

        /** Nothing open. Discards unsaved edits — the caller is what asks first. */
        void Close();

        /** Take the current tree as the new baseline. Called after a successful Save. */
        void MarkSaved();

        /** Replace the whole tree from text — how an undo step restores one (**UI15**). */
        bool RestoreFrom(const OpaaxString& InText, const UIWidgetRegistry& InRegistry);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** Just the file name, for the panel title — "Hud.opaaxui". Empty when none is open. */
        OpaaxString FileName() const;

        UICanvas&       GetCanvas()       noexcept { return m_Canvas; }
        const UICanvas& GetCanvas() const noexcept { return m_Canvas; }

        /** The tree as it would be written. What IsDirty compares and what an undo step carries. */
        OpaaxString Serialize() const;

        /** Whether the tree differs from what was last written. Recomputed, never cached. */
        bool IsDirty() const;

        // =============================================================================
        // Selection — a PATH, so a rebuilt tree cannot leave it dangling
        // =============================================================================
    public:
        const UIWidgetPath& SelectedPath() const noexcept { return m_Selected; }
        void                Select(UIWidgetPath InPath) { m_Selected = Move(InPath); }
        void                ClearSelection() { m_Selected.clear(); }

        /** The selected widget, or null when the path names nothing (a deleted node). */
        UIWidget* SelectedWidget();

        /** The widget InPath names, or null. An empty path is the root. */
        UIWidget* Resolve(const UIWidgetPath& InPath);

        /** InWidget's path from the root. Empty when it is the root OR not in this tree. */
        UIWidgetPath PathOf(const UIWidget& InWidget) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString  m_AbsPath;
        UICanvas     m_Canvas;
        UIWidgetPath m_Selected;

        /** The serialized text as of the last Open/Save — what IsDirty compares against. */
        OpaaxString m_Baseline;
    };
}
