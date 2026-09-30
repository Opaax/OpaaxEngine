#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/EditorUICanvasDocument.h"   // UIWidgetPath

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // UICanvasOps — the UI canvas editor's actions. Each changes the open document and records an
    //   undo step holding the whole tree before and after. A gesture takes the text before, changes
    //   the tree, then records with the text after.
    // =============================================================================
    namespace UICanvasOps
    {
        /** The current tree as text (taken before a gesture starts). */
        OpaaxString Snapshot(EditorContext& InContext);

        /**
         * Adds a widget of type InType under InParent.
         * @return The new widget's path. Empty when nothing is open or the type is not registered
         */
        UIWidgetPath AddWidget(EditorContext& InContext, OpaaxStringID InType, const UIWidgetPath& InParent);

        /** Removes the widget at InPath and its subtree. The root is refused. */
        bool RemoveWidget(EditorContext& InContext, const UIWidgetPath& InPath);

        /**
         * Moves the widget at InPath under InNewParent. A move into its own subtree is refused.
         */
        bool ReparentWidget(EditorContext& InContext, const UIWidgetPath& InPath, const UIWidgetPath& InNewParent);

        /** Moves the widget at InPath by InDelta places among its siblings (clamped). Sibling order is draw order. */
        bool MoveWidget(EditorContext& InContext, const UIWidgetPath& InPath, Int32 InDelta);

        /** Copies the widget at InPath, placed right after it, named "<Name> (1)". @return Its path, empty on failure */
        UIWidgetPath DuplicateWidget(EditorContext& InContext, const UIWidgetPath& InPath);

        /** The widget at InPath as text (for the clipboard). Empty when there is none. */
        OpaaxString CopyWidget(EditorContext& InContext, const UIWidgetPath& InPath);

        /** Pastes a subtree from text under InParent. @return Its path, empty when the text is not a widget */
        UIWidgetPath PasteWidget(EditorContext& InContext, const OpaaxString& InText, const UIWidgetPath& InParent);

        /**
         * Closes an in-place property edit (the panel's DrawProperties).
         * @param InBefore The whole tree when the edit started (Snapshot)
         * @return True if a step was recorded, false if nothing changed
         */
        bool CommitEdit(EditorContext& InContext, const OpaaxString& InBefore, const char* InLabel);

        /** Writes the open canvas to its file, updates the dirty baseline, and reloads the resource. */
        bool Save(EditorContext& InContext);
    }
}
