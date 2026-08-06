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
    //   surfaces can grow here without touching the runtime Paths.
    //
    //   Editor space — a per-project editor data root that MIRRORS the project layout, one level down:
    //
    //       <ProjectRoot>/Editor/
    //           Assets/  Configs/  Source/  Save/  Temp/
    //
    //   for editor-only assets, settings, dock layouts, scratch saves, etc. Derived from the inherited
    //   ProjectRoot() — it adds NO surface to the engine's IPaths (D4: the engine never learns the editor
    //   exists), so these methods live only on this concrete type in OpaaxEditorLib.
    // =============================================================================
    class EditorPaths final : public Paths
    {
    public:
        // =============================================================================
        // CTOR
        // =============================================================================
        /***/
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

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin Paths interface
        void LogPaths() const override;
        //~End Paths interface
    };
}
