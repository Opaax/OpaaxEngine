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
    //   Tool space — Editor/ in the engine's repository, the editor's own content:
    //
    //       EditorAssetsDir()  <ProjectRoot>/Editor/Assets        this project's editor content
    //       ToolAssetsDir()    <engine repository>/Editor/Assets  the editor's own content (type icons)
    //
    //   Tool space depends on neither the project nor the workspace, so it works even with no edited
    //   project, and in a game repository that holds the engine in a subdirectory.
    // =============================================================================
    class EditorPaths final : public Paths
    {
    public:
        // =============================================================================
        // CTOR
        // =============================================================================
        EditorPaths(const IPlatform& InPlatform, const CommandLine& InCommandLine, const OpaaxString& InProjectRel)
            : Paths(InPlatform, InCommandLine, InProjectRel)
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

        OpaaxString ToolDir()          const; // <engine repository>/Editor
        OpaaxString ToolAssetsDir()    const; // <engine repository>/Editor/Assets

        OpaaxString ToolAssetToAbsolute(const OpaaxString& InAssetRel) const; // under <engine repository>/Editor/Assets

    };
}
