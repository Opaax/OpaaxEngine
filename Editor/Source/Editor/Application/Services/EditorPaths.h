#pragma once

#include "Application/Services/IPaths.h"

namespace Opaax::Editor
{
    // =============================================================================
    // EditorPaths — the editor's IPaths. An editor exe (SandboxEditor.exe) is not named like the
    //   project it edits, so EditorPaths targets the edited project given by the editor host
    //   (EditorApplication::GetEditedProjectName), under the source workspace.
    //
    //   Editor space — per-project editor data, same layout one level down:
    //
    //       <ProjectRoot>/Editor/
    //           Assets/  Configs/  Source/  Save/  Temp/
    //
    //   Tool space — <WorkspaceRoot>/Editor, the editor's own content:
    //
    //       EditorAssetsDir()  <ProjectRoot>/Editor/Assets    this project's editor content
    //       ToolAssetsDir()    <WorkspaceRoot>/Editor/Assets  the editor's own content (type icons)
    //
    //   Tool space does not depend on the project, so it works even with no edited project.
    // =============================================================================
    class EditorPaths final : public Paths
    {
    public:
        // =============================================================================
        // CTOR
        // =============================================================================
        EditorPaths(const IPlatform& InPlatform, int InArgc, char** InArgv, const OpaaxString& InProjectRel)
            : Paths(InPlatform, InArgc, InArgv, InProjectRel)
        {
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        OpaaxString EditorDir()        const; // <ProjectRoot>/Editor
        OpaaxString EditorAssetsDir()  const; // <ProjectRoot>/Editor/Assets
        OpaaxString EditorConfigsDir() const; // <ProjectRoot>/Editor/Configs
        OpaaxString EditorSourceDir()  const; // <ProjectRoot>/Editor/Source
        OpaaxString EditorSaveDir()    const; // <ProjectRoot>/Editor/Save
        OpaaxString EditorTempDir()    const; // <ProjectRoot>/Editor/Temp
        
        OpaaxString EditorToAbsolute(const OpaaxString& InEditorRel)     const; // under <ProjectRoot>/Editor
        OpaaxString EditorAssetToAbsolute(const OpaaxString& InAssetRel) const; // under <ProjectRoot>/Editor/Assets

        OpaaxString ToolDir()          const; // <WorkspaceRoot>/Editor
        OpaaxString ToolAssetsDir()    const; // <WorkspaceRoot>/Editor/Assets

        OpaaxString ToolAssetToAbsolute(const OpaaxString& InAssetRel) const; // under <WorkspaceRoot>/Editor/Assets

    };
}
