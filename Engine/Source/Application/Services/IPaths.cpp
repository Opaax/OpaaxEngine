#include "Application/Services/IPaths.h"

#include "Core/IO/FileSystem.h"
#include "Core/Log/Logger.h"
#include "Core/String/CString.h"
#include "Core/String/OpaaxPathString.h"
#include "Platform/IPlatform.h"

namespace Opaax
{
    namespace
    {
        OpaaxString FindProjectArg(int InArgc, char** InArgv)
        {
            for (int i = 1; i + 1 < InArgc; ++i)
            {
                if (CString::Equals(InArgv[i], "--project"))
                {
                    return OpaaxString(InArgv[i + 1]);
                }
            }
            return OpaaxString();
        }

        // =====================================================================
        // NullPaths — every path empty.
        // =====================================================================
        class NullPaths final : public IPaths
        {
        public:
            bool        IsNull()        const noexcept override { return true; }
            OpaaxString WorkspaceRoot() const override { return OpaaxString(); }
            OpaaxString EngineRoot()    const override { return OpaaxString(); }
            OpaaxString ProjectRoot()   const override { return OpaaxString(); }
            OpaaxString ProjectFile()   const override { return OpaaxString(); }
            OpaaxString AssetsDir()     const override { return OpaaxString(); }
            OpaaxString ConfigsDir()    const override { return OpaaxString(); }
            OpaaxString SourceDir()     const override { return OpaaxString(); }
            OpaaxString SaveDir()       const override { return OpaaxString(); }
            OpaaxString TempDir()       const override { return OpaaxString(); }
            OpaaxString EngineToAbsolute(const OpaaxString&)  const override { return OpaaxString(); }
            OpaaxString ProjectToAbsolute(const OpaaxString&) const override { return OpaaxString(); }
            OpaaxString AssetToAbsolute(const OpaaxString&)   const override { return OpaaxString(); }
            OpaaxString AbsoluteToAsset(const OpaaxString&)   const override { return OpaaxString(); }
            void        LogPaths()                            const override { OPAAX_APP_LOG(Warn, "Null Path Service"); }
        };

        // InAbsPath relative to InRoot, or empty if not under it.
        // Both are normalized first (slashes differ; the file may not exist yet).
        OpaaxString RelativeUnder(const OpaaxString& InRoot, const OpaaxString& InAbsPath)
        {
            if (InRoot.IsEmpty())
            {
                return OpaaxString();
            }

            const OpaaxString lAbs  = FileSystem::MakeCanonical(InAbsPath);
            const OpaaxString lRoot = FileSystem::MakeCanonical(InRoot);

            if (lAbs.IsEmpty() || lRoot.IsEmpty())
            {
                return OpaaxString();
            }

            const OpaaxString lRelative = PathString::RelativeTo(lAbs, lRoot);

            // Empty or starting with "..": not under the root.
            if (lRelative.IsEmpty() || lRelative == ".." || lRelative.StartsWith("../"))
            {
                return OpaaxString();
            }

            return lRelative;
        }
    }

    // =========================================================================
    // Pure resolver
    // =========================================================================
    ProjectLayout ResolveProjectLayout(const OpaaxString& InExePath,
                                       const OpaaxString& InWorkspaceDir,
                                       const OpaaxString& InProjectArg)
    {
        const OpaaxString lAppName = PathString::Stem(InExePath).ToString();

        // No workspace (release): use the exe directory.
        const OpaaxString lWorkspace = InWorkspaceDir.IsEmpty()
                                           ? PathString::Parent(InExePath)
                                           : PathString::Generic(InWorkspaceDir);

        // --project (relative to the workspace), else <workspace>/<AppName>/<AppName>.opaaxproj in a
        // dev build and <exe dir>/<AppName>_Data/<AppName>.opaaxproj in a ship build.
        OpaaxString lProjFile;
        if (!InProjectArg.IsEmpty())
        {
            lProjFile = PathString::IsAbsolute(InProjectArg) ? InProjectArg : PathString::Join(lWorkspace, InProjectArg);
        }
        else
        {
            const OpaaxString lProjDir = InWorkspaceDir.IsEmpty() ? lAppName + SHIPPED_PROJECT_DIR_SUFFIX : lAppName;
            lProjFile = PathString::Join(PathString::Join(lWorkspace, lProjDir), lAppName + ".opaaxproj");
        }
        lProjFile = PathString::Normalize(lProjFile);

        const OpaaxString lProjRoot = PathString::Parent(lProjFile);

        ProjectLayout lOut;
        lOut.WorkspaceRoot = lWorkspace;
        lOut.EngineRoot    = PathString::Join(lWorkspace, "Engine");
        lOut.ProjectRoot   = lProjRoot;
        lOut.ProjectFile   = lProjFile;
        lOut.AssetsDir     = PathString::Join(lProjRoot, "Assets");
        lOut.ConfigsDir    = PathString::Join(lProjRoot, "Configs");
        lOut.SourceDir     = PathString::Join(lProjRoot, "Source");
        lOut.SaveDir       = PathString::Join(lProjRoot, "Save");
        lOut.TempDir       = PathString::Join(lProjRoot, "Temp");
        return lOut;
    }

    // =========================================================================
    // Type tag + null object (defined here so there is one of each).
    // =========================================================================
    ServiceTypeID IPaths::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IPaths& IPaths::Null()
    {
        static NullPaths s_Null;
        return s_Null;
    }

    OpaaxString IPaths::EngineAssetsDir() const
    {
        return EngineToAbsolute(OpaaxString("Assets"));
    }

    // =========================================================================
    // Paths
    // =========================================================================
    Paths::Paths(const IPlatform& InPlatform, int InArgc, char** InArgv,
                 const OpaaxString& InProjectOverride)
    {
        OpaaxString lWorkspace;
        
#if defined(OPAAX_WORKSPACE_DIR)
        lWorkspace = OpaaxString(OPAAX_WORKSPACE_DIR);
#endif
        const OpaaxString lExe = InPlatform.GetExecutablePath();

        // --project first, then the host's project, else the default from the exe name.
        OpaaxString lProjArg = FindProjectArg(InArgc, InArgv);
        if (lProjArg.IsEmpty())
        {
            lProjArg = InProjectOverride;
        }

        m_Layout = ResolveProjectLayout(lExe, lWorkspace, lProjArg);
    }

    void Paths::LogPaths() const
    {
        OPAAX_APP_LOG(Info, "Project root '{}', engine root '{}'", ProjectRoot().CStr(), EngineRoot().CStr());
    }

    OpaaxString Paths::EngineToAbsolute(const OpaaxString& InEngineRel) const
    {
        return PathString::Join(m_Layout.EngineRoot, InEngineRel);
    }

    OpaaxString Paths::ProjectToAbsolute(const OpaaxString& InProjectRel) const
    {
        return PathString::Join(m_Layout.ProjectRoot, InProjectRel);
    }

    OpaaxString Paths::AssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        const OpaaxStringView lRef(InAssetRel);
        if (lRef.StartsWith(ENGINE_MOUNT))
        {
            const OpaaxString lEngineRel = lRef.SubString(OpaaxStringView(ENGINE_MOUNT).GetLength()).ToString();
            return PathString::Join(EngineAssetsDir(), lEngineRel);
        }

        return PathString::Join(m_Layout.AssetsDir, InAssetRel);
    }

    OpaaxString Paths::AbsoluteToAsset(const OpaaxString& InAbsPath) const
    {
        if (InAbsPath.IsEmpty())
        {
            return OpaaxString();
        }

        // Project first: its paths have no prefix.
        if (const OpaaxString lProjectRel = RelativeUnder(m_Layout.AssetsDir, InAbsPath); !lProjectRel.IsEmpty())
        {
            return lProjectRel;
        }

        if (const OpaaxString lEngineRel = RelativeUnder(EngineAssetsDir(), InAbsPath); !lEngineRel.IsEmpty())
        {
            return OpaaxString(ENGINE_MOUNT) + lEngineRel;
        }

        return OpaaxString();
    }
}
