#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Editor/EditorUICanvasDocument.h"   // UIWidgetPath — every verb addresses a node by one

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // UICanvasOps — the UI canvas editor's VERBS, beside FamilyOps, LibraryOps and ClipOps.
    //
    //   Each one mutates the open document and records its own undo step (**UN1**), and every step
    //   is the WHOLE TREE before and after (**UI15**) — a tree cannot be half-restored.
    //
    //   THE SHAPE OF A GESTURE: take the text BEFORE, mutate, then Record with the text AFTER. The
    //   panel's property editing does the same across frames, which is why CommitEdit takes the
    //   before-text rather than computing it.
    // =============================================================================
    namespace UICanvasOps
    {
        /** The tree as it stands — a gesture takes this before it starts writing. */
        OpaaxString Snapshot(EditorContext& InContext);

        /**
         * Add a widget of type InType under the widget InParent names.
         *
         * @return the new widget's path; EMPTY when nothing is open or the type is not registered
         *   (an empty path is the ROOT, which is never what an add returns).
         */
        UIWidgetPath AddWidget(EditorContext& InContext, OpaaxStringID InType, const UIWidgetPath& InParent);

        /** Remove the widget InPath names, and its subtree. Refuses the root. */
        bool RemoveWidget(EditorContext& InContext, const UIWidgetPath& InPath);

        /**
         * Move the widget at InPath under the one at InNewParent.
         *
         * Refuses a move INTO the subtree being moved — a cycle is a tree that no longer terminates,
         * which the hierarchy's own drop refuses for the same reason (**HR**).
         */
        bool ReparentWidget(EditorContext& InContext, const UIWidgetPath& InPath, const UIWidgetPath& InNewParent);

        /**
         * Close an in-place property edit — the panel's DrawProperties gesture.
         *
         * @param InBefore The whole tree as it was when the gesture opened (Snapshot).
         * @return true when a step was recorded; false when the gesture changed nothing.
         */
        bool CommitEdit(EditorContext& InContext, const OpaaxString& InBefore, const char* InLabel);

        /** Write the open canvas back to its file, rebase the dirty marker and publish it. */
        bool Save(EditorContext& InContext);
    }
}
