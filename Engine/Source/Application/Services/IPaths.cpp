#include "Application/Services/IPaths.h"
#include "Core/Log/Logger.h"
#include "Platform/IPlatform.h"

#include "Core/String/OpaaxUtf8.h"

#include <cstring>
#include <filesystem>

namespace Opaax
{
    namespace
    {
        namespace fs = std::filesystem;

        OpaaxString FindProjectArg(int InArgc, char** InArgv)
        {
            for (int i = 1; i + 1 < InArgc; ++i)
            {
                if (std::strcmp(InArgv[i], "--project") == 0)
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

            std::error_code lError;
            const fs::path lAbs  = fs::weakly_canonical(Utf8::ToFsPath(InAbsPath), lError);
            const fs::path lRoot = fs::weakly_canonical(Utf8::ToFsPath(InRoot), lError);

            if (lError)
            {
                return OpaaxString();
            }

            const fs::path lRelative = lAbs.lexically_relative(lRoot);

            // Empty or starting with "..": not under the root.
            if (lRelative.empty() || *lRelative.begin() == "..")
            {
                return OpaaxString();
            }

            return Utf8::FromFsPath(lRelative);
        }
    }

    // =========================================================================
    // Pure resolver
    // =========================================================================
    ProjectLayout ResolveProjectLayout(const OpaaxString& InExePath,
                                       const OpaaxString& InWorkspaceDir,
                                       const OpaaxString& InProjectArg)
    {
        const fs::path lExe     = Utf8::ToFsPath(InExePath);
        const fs::path lExeDir  = lExe.parent_path();

        // Kept as fs::path: .string() would break non-ASCII names.
        const fs::path lAppName = lExe.stem();

        // No workspace (release): use the exe directory.
        const fs::path lWorkspace = InWorkspaceDir.IsEmpty()
                                        ? lExeDir
                                        : Utf8::ToFsPath(InWorkspaceDir);

        // --project (relative to the workspace), else <workspace>/<AppName>/<AppName>.opaaxproj.
        fs::path lProjFile;
        if (!InProjectArg.IsEmpty())
        {
            const fs::path lArg = Utf8::ToFsPath(InProjectArg);
            lProjFile = lArg.is_absolute() ? lArg : (lWorkspace / lArg);
        }
        else
        {
            fs::path lLeaf = lAppName;
            lLeaf += ".opaaxproj";
            lProjFile = lWorkspace / lAppName / lLeaf;
        }
        lProjFile = lProjFile.lexically_normal();

        const fs::path lProjRoot = lProjFile.parent_path();

        ProjectLayout lOut;
        lOut.WorkspaceRoot = Utf8::FromFsPath(lWorkspace);
        lOut.EngineRoot    = Utf8::FromFsPath(lWorkspace / "Engine");
        lOut.ProjectRoot   = Utf8::FromFsPath(lProjRoot);
        lOut.ProjectFile   = Utf8::FromFsPath(lProjFile);
        lOut.AssetsDir     = Utf8::FromFsPath(lProjRoot / "Assets");
        lOut.ConfigsDir    = Utf8::FromFsPath(lProjRoot / "Configs");
        lOut.SourceDir     = Utf8::FromFsPath(lProjRoot / "Source");
        lOut.SaveDir       = Utf8::FromFsPath(lProjRoot / "Save");
        lOut.TempDir       = Utf8::FromFsPath(lProjRoot / "Temp");
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
        return Utf8::FromFsPath(Utf8::ToFsPath(m_Layout.EngineRoot) / Utf8::ToFsPath(InEngineRel));
    }

    OpaaxString Paths::ProjectToAbsolute(const OpaaxString& InProjectRel) const
    {
        return Utf8::FromFsPath(Utf8::ToFsPath(m_Layout.ProjectRoot) / Utf8::ToFsPath(InProjectRel));
    }

    OpaaxString Paths::AssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        const OpaaxStringView lRef(InAssetRel);
        if (lRef.StartsWith(ENGINE_MOUNT))
        {
            const OpaaxString lEngineRel = lRef.SubString(OpaaxStringView(ENGINE_MOUNT).GetLength()).ToString();
            return Utf8::FromFsPath(Utf8::ToFsPath(EngineAssetsDir()) / Utf8::ToFsPath(lEngineRel));
        }

        return Utf8::FromFsPath(Utf8::ToFsPath(m_Layout.AssetsDir) / Utf8::ToFsPath(InAssetRel));
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
