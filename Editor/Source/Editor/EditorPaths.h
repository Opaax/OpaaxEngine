#pragma once

#include "Application/Services/IPaths.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorPaths — the editor's IPaths (Editor.md, user decision 2026-07-17). An editor exe is a
    //   separate binary from the game it edits (SandboxEditor.exe vs the Sandbox project), so the
    //   exe-stem default would resolve to a nonexistent "<EditorExe>" project. EditorPaths instead
    //   targets the EDITED project, supplied by the editor host (EditorApplication::GetEditedProjectName),
    //   under the source workspace. A distinct type (not just Paths-with-a-param) so editor-only path
    //   surfaces (layout/prefs/scene-save dirs) can grow here without touching the runtime Paths.
    // =============================================================================
    class EditorPaths final : public Paths
    {
    public:
        // InProjectRel: the edited project's path relative to the workspace, e.g. "Sandbox/Sandbox.opaaxproj".
        EditorPaths(const IPlatform& InPlatform, int InArgc, char** InArgv, const OpaaxString& InProjectRel)
            : Paths(InPlatform, InArgc, InArgv, InProjectRel)
        {
        }
    };
}
