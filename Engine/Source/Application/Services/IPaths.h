#pragma once

#include "IAppService.h"
#include "Core/String/OpaaxString.hpp"

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
    // Mounts — what an ASSET REFERENCE ("Textures/Hero.png") is relative to.
    //
    // Unprefixed is the project's own Assets dir, which is what every existing .opaaxmap and
    // .opaaxlevel already writes. A leading '/' names a mount instead — the one discriminator
    // that cannot collide, since a relative asset name never starts with one.
    //
    // There is deliberately no symmetric "/Game/" for project content: introducing one would
    // rewrite every map file on disk (MP6 verifies them byte-for-byte) to say what the absence
    // of a prefix already says.
    // =============================================================================
    inline constexpr const char* ENGINE_MOUNT = "/Engine/";

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
        
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void LogPaths() const = 0; 

        //----- roots ----------------------------------------------------------
        virtual OpaaxString WorkspaceRoot() const = 0; // editor: source tree; release: exe dir
        virtual OpaaxString EngineRoot()    const = 0; // <WorkspaceRoot>/Engine

        //----- project layout -------------------------------------------------
        virtual OpaaxString ProjectRoot()   const = 0;   // dir holding the .opaaxproj
        virtual OpaaxString ProjectFile()   const = 0;   // the .opaaxproj itself
        virtual OpaaxString AssetsDir()     const = 0; 
        virtual OpaaxString ConfigsDir()    const = 0;
        virtual OpaaxString SourceDir()     const = 0;
        virtual OpaaxString SaveDir()       const = 0;
        virtual OpaaxString TempDir()       const = 0;

        /**
         * <EngineRoot>/Assets — the content the ENGINE ships, and what ENGINE_MOUNT resolves against.
         *
         * Non-virtual: it is EngineToAbsolute("Assets") and nothing else, so stating it once here
         * spares every IPaths implementation (the null object, three test doubles) an override that
         * could only repeat the same line.
         */
        OpaaxString EngineAssetsDir() const;

        //----- resolvers ------------------------------------------------------
        virtual OpaaxString EngineToAbsolute(const OpaaxString& InEngineRel)   const = 0; // under EngineRoot
        virtual OpaaxString ProjectToAbsolute(const OpaaxString& InProjectRel) const = 0; // under ProjectRoot
        virtual OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel)     const = 0; // under AssetsDir, or a mount

        /**
         * The inverse of AssetToAbsolute: an absolute path back to the form an asset is REFERENCED
         * by ("Maps/Main.opaaxmap", "/Engine/Textures/T_Checker_64.png"), with forward slashes
         * whatever the input used.
         *
         * Exists because the editor authors asset references — a level manifest names its maps
         * asset-relative, and a file dialog hands back an absolute native path.
         *
         * @return EMPTY when InAbsPath is under NO mount. That is a real answer, not a failure:
         *   a file from elsewhere cannot be named by a manifest at all.
         */
        virtual OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath)      const = 0;

        //----- null object ----------------------------------------------------
        static IPaths& Null();
    };

    // =============================================================================
    // Paths — wires IPlatform + argv + the build-time workspace into ResolveProjectLayout.
    //   Base anchor : IPlatform::GetExecutablePath() (NOT argv[0]).
    //   Workspace   : OPAAX_WORKSPACE_DIR in editor builds; the exe dir in release.
    //   Not final — EditorPaths (OpaaxEditorLib) subclasses it to target the EDITED project rather
    //   than the editor exe's own name (D8: SandboxEditor.exe edits the Sandbox project).
    // =============================================================================
    class OPAAX_API Paths : public IPaths
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        Paths(const IPlatform& InPlatform, int InArgc, char** InArgv,
              const OpaaxString& InProjectOverride = OpaaxString());

        // =============================================================================
        // Override
        // =============================================================================
        //~ Begin IPaths interface
    public:
        virtual void LogPaths() const override;
        
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
        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath)      const override;
        //~ End IPaths interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ProjectLayout m_Layout;
    };
}
