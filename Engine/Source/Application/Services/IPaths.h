#pragma once

#include "Application/Services/IAppService.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class IPlatform;

    // =============================================================================
    // ProjectLayout — resolved engine and project directories.
    // =============================================================================
    struct ProjectLayout
    {
        OpaaxString WorkspaceRoot;   // editor: source workspace; release: exe dir
        OpaaxString EngineRoot;      // <WorkspaceRoot>/Engine
        OpaaxString ProjectRoot;     // dir holding the .opaaxproj
        OpaaxString ProjectFile;     // the .opaaxproj itself
        OpaaxString AssetsDir;       // <ProjectRoot>/Assets
        OpaaxString ConfigsDir;      // <ProjectRoot>/Configs   
        OpaaxString SourceDir;       // <ProjectRoot>/Source 
        OpaaxString SaveDir;         // <ProjectRoot>/Save 
        OpaaxString TempDir;         // <ProjectRoot>/Temp 
    };

    /**
     * Resolves every engine and project directory. No OS calls.
     * @param InExePath Absolute path to the running executable
     * @param InWorkspaceDir Source workspace in dev builds; empty in release (uses the exe directory)
     * @param InProjectArg Value of --project (relative to the workspace), or empty for
     *   <WorkspaceRoot>/<ExeName>/<ExeName>.opaaxproj
     */
    ProjectLayout ResolveProjectLayout(const OpaaxString& InExePath,
                                                 const OpaaxString& InWorkspaceDir,
                                                 const OpaaxString& InProjectArg);

    // =============================================================================
    // Mounts — an asset path without prefix is relative to the project's Assets dir.
    //   "/Engine/..." is relative to the engine's Assets dir.
    // =============================================================================
    inline constexpr const char* ENGINE_MOUNT = "/Engine/";

    // =============================================================================
    // IPaths — engine and project directories.
    //
    //     <ProjectRoot>/
    //         Assets/  Configs/  Source/  Save/  Temp/
    //         <Name>.opaaxproj
    // =============================================================================
    class IPaths : public IAppService
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
        virtual OpaaxString WorkspaceRoot() const = 0; // dev: source tree; release: exe dir
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
         * @return <EngineRoot>/Assets, where "/Engine/" asset paths point
         */
        OpaaxString EngineAssetsDir() const;

        //----- resolvers ------------------------------------------------------
        virtual OpaaxString EngineToAbsolute(const OpaaxString& InEngineRel)   const = 0; // under EngineRoot
        virtual OpaaxString ProjectToAbsolute(const OpaaxString& InProjectRel) const = 0; // under ProjectRoot
        virtual OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel)     const = 0; // under AssetsDir, or a mount

        /**
         * Inverse of AssetToAbsolute ("Maps/Main.opaaxmap", "/Engine/Textures/T_Checker_64.png").
         * @return Empty if the path is outside every Assets dir
         */
        virtual OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath)      const = 0;

        //----- null object ----------------------------------------------------
        static IPaths& Null();
    };

    // =============================================================================
    // Paths — resolves the layout from the executable path, --project and the workspace.
    //   EditorPaths derives from it to point at the edited project.
    // =============================================================================
    class Paths : public IPaths
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
