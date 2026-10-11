#pragma once

namespace Opaax
{
    class AutomationRunner;
}

namespace Opaax::Editor
{
    struct EditorContext;
    class GuardedEditorDialogs;

    // =============================================================================
    // EditorAutomation — the editor's commands for an agent driving it, on top of the engine's:
    //   Play In Editor, undo, levels and maps, creating, destroying and selecting entities, and
    //   component edits that are undoable like the Inspector's. They go through the same commands
    //   and operations as the editor's own UI, so an agent's change is the same as a user's.
    // =============================================================================
    namespace EditorAutomation
    {
        /**
         * Adds the editor's commands to InRunner. While one runs, InDialogs stay closed: a request that
         * met a dialog fails, saying what was asked.
         */
        void Register(AutomationRunner& InRunner, EditorContext& InContext, GuardedEditorDialogs& InDialogs);
    }
}
