#pragma once

#include "Core/Log/Logger.h"
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
     * A widget's position as child indices from the root: {0, 2} is the root's first child's third
     * child; the root is the empty path. Used instead of pointers because an undo replaces the whole
     * tree.
     */
    using UIWidgetPath = TDynArray<Uint32>;

    // =============================================================================
    // EditorUICanvasDocument — the open .opaaxui, its canvas, the selection, and whether it changed
    //   since the last save. Owns a real UICanvas: the panel previews it by rendering it.
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
        /** Reads InAbsPath and makes it the open document. On failure the previous one stays open. */
        bool Open(const OpaaxString& InAbsPath, const UIWidgetRegistry& InRegistry);

        /** Closes the document. Discards unsaved edits (the caller asks first). */
        void Close();

        /** Takes the current tree as the new baseline. Called after a successful Save. */
        void MarkSaved();

        /** Replaces the whole tree from text (used by undo). */
        bool RestoreFrom(const OpaaxString& InText, const UIWidgetRegistry& InRegistry);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool               IsOpen()  const noexcept { return !m_AbsPath.IsEmpty(); }
        const OpaaxString& AbsPath() const noexcept { return m_AbsPath; }

        /** The file name, for the panel title ("Hud.opaaxui"). Empty when none is open. */
        OpaaxString FileName() const;

        UICanvas&       GetCanvas()       noexcept { return m_Canvas; }
        const UICanvas& GetCanvas() const noexcept { return m_Canvas; }

        /** The tree as it would be written (compared by IsDirty, carried by undo steps). */
        OpaaxString Serialize() const;

        /** Whether the tree differs from what was last written. Recomputed each time. */
        bool IsDirty() const;

        // =============================================================================
        // Selection — a path, so a rebuilt tree cannot leave it dangling
        // =============================================================================
    public:
        const UIWidgetPath& SelectedPath() const noexcept { return m_Selected; }
        void                Select(UIWidgetPath InPath) { m_Selected = Move(InPath); }
        void                ClearSelection() { m_Selected.clear(); }

        /** The selected widget, or null if the path names nothing. */
        UIWidget*       SelectedWidget();
        const UIWidget* SelectedWidget() const { return Resolve(m_Selected); }

        /** The widget at InPath, or null. An empty path is the root. */
        UIWidget*       Resolve(const UIWidgetPath& InPath);
        const UIWidget* Resolve(const UIWidgetPath& InPath) const;

        /** InWidget's path from the root. Empty for the root or a widget not in this tree. */
        UIWidgetPath PathOf(const UIWidget& InWidget) const;

        /**
         * The widget a click at InCanvasPoint selects: the deepest visible one, last drawn first.
         * Unlike HitTest, ignores bHitTestable (panels and masks must be selectable). Empty = the root.
         */
        UIWidgetPath PickAt(const Vector2F& InCanvasPoint) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxString  m_AbsPath;
        UICanvas     m_Canvas;
        UIWidgetPath m_Selected;

        /** The serialized text at the last Open/Save (what IsDirty compares against). */
        OpaaxString m_Baseline;
    };
}
