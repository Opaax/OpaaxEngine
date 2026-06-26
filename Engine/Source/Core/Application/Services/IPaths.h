#pragma once

#include "IAppService.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    class IPlatform;

    // =============================================================================
    // ProjectLayout — fully-resolved roots. Plain data, produced by ResolveProjectLayout
    // so resolution stays pure + unit-testable, independent of any build-time define.
    // =============================================================================
    struct ProjectLayout
    {
        OpaaxString WorkspaceRoot;   // editor: source workspace; release: exe dir
        OpaaxString EngineRoot;      // <WorkspaceRoot>/Engine
        OpaaxString ProjectRoot;     // dir holding the .opaaxproj
        OpaaxString ProjectFile;     // the .opaaxproj itself
        OpaaxString AssetsDir;       // <ProjectRoot>/Assets   (+ Configs/Source/Save/Temp)
        OpaaxString ConfigsDir;      // <ProjectRoot>/Configs   
        OpaaxString SourceDir;       // <ProjectRoot>/Source 
        OpaaxString SaveDir;         // <ProjectRoot>/Save 
        OpaaxString TempDir;         // <ProjectRoot>/Temp 
    };

    // Pure resolver — no OS calls, no globals, no defines. THE source of path truth.
    //   InExePath      : absolute path to the running binary (IPlatform::GetExecutablePath()).
    //   InWorkspaceDir : EDITOR = the source workspace root (OPAAX_WORKSPACE_DIR); RELEASE = "".
    //                    Empty => WorkspaceRoot falls back to the executable's directory.
    //   InProjectArg   : `--project <path>` value (absolute kept; relative resolved under the
    //                    workspace), or "" => default <WorkspaceRoot>/<exeStem>/<exeStem>.opaaxproj.

    /**
     * Pure resolver — no OS calls, no globals, no defines. THE source of path truth.
     * @param InExePath absolute path to the running binary (IPlatform::GetExecutablePath()).
     * @param InWorkspaceDir  EDITOR = the source workspace root (OPAAX_WORKSPACE_DIR); RELEASE = "".
     * @param InProjectArg `--project <path>` value (absolute kept; relative resolved under the workspace), or "" => default <WorkspaceRoot>/<exeStem>/<exeStem>.opaaxproj.
     * @return Project layout with path
     */
    OPAAX_API ProjectLayout ResolveProjectLayout(const OpaaxString& InExePath,
                                                 const OpaaxString& InWorkspaceDir,
                                                 const OpaaxString& InProjectArg);

    // =============================================================================
    // IPaths — resolved engine + project layout.
    //
    // ProjectRoot is the directory that holds the .opaaxproj; every project directory
    // is derived from it BY CONVENTION (no path is ever stored in the project file):
    //
    //     <ProjectRoot>/
    //         Assets/  Configs/  Source/  Save/  Temp/
    //         <Name>.opaaxproj
    //
    // Standalone locator service (not a Platform facet); consumes IPlatform for the
    // executable path that anchors the roots.
    // =============================================================================
    class OPAAX_API IPaths : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IPaths)

        //----- roots ----------------------------------------------------------
        virtual OpaaxString WorkspaceRoot() const = 0; // editor: source tree; release: exe dir
        virtual OpaaxString EngineRoot()    const = 0; // <WorkspaceRoot>/Engine

        //----- project layout -------------------------------------------------
        virtual OpaaxString ProjectRoot() const = 0;   // dir holding the .opaaxproj
        virtual OpaaxString ProjectFile() const = 0;   // the .opaaxproj itself
        virtual OpaaxString AssetsDir()   const = 0;
        virtual OpaaxString ConfigsDir()  const = 0;
        virtual OpaaxString SourceDir()   const = 0;
        virtual OpaaxString SaveDir()     const = 0;
        virtual OpaaxString TempDir()      const = 0;

        //----- resolvers ------------------------------------------------------
        virtual OpaaxString EngineToAbsolute(const OpaaxString& InEngineRel)   const = 0; // under EngineRoot
        virtual OpaaxString ProjectToAbsolute(const OpaaxString& InProjectRel) const = 0; // under ProjectRoot
        virtual OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel)     const = 0; // under AssetsDir

        //----- null object ----------------------------------------------------
        static IPaths& Null();
    };

    // =============================================================================
    // Paths — wires IPlatform + argv + the build-time workspace into ResolveProjectLayout.
    //   Base anchor : IPlatform::GetExecutablePath() (NOT argv[0]).
    //   Workspace   : OPAAX_WORKSPACE_DIR in editor builds; the exe dir in release.
    // =============================================================================
    class OPAAX_API Paths final : public IPaths
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        Paths(const IPlatform& InPlatform, int InArgc, char** InArgv);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        OpaaxString WorkspaceRoot() const override { return m_Layout.WorkspaceRoot; }
        OpaaxString EngineRoot()    const override { return m_Layout.EngineRoot; }
        OpaaxString ProjectRoot()   const override { return m_Layout.ProjectRoot; }
        OpaaxString ProjectFile()   const override { return m_Layout.ProjectFile; }
        OpaaxString AssetsDir()     const override { return m_Layout.AssetsDir; }
        OpaaxString ConfigsDir()    const override { return m_Layout.ConfigsDir; }
        OpaaxString SourceDir()     const override { return m_Layout.SourceDir; }
        OpaaxString SaveDir()       const override { return m_Layout.SaveDir; }
        OpaaxString TempDir()       const override { return m_Layout.TempDir; }

        OpaaxString EngineToAbsolute(const OpaaxString& InEngineRel)   const override;
        OpaaxString ProjectToAbsolute(const OpaaxString& InProjectRel) const override;
        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel)     const override;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ProjectLayout m_Layout;
    };
}
