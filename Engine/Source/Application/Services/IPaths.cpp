#include "IPaths.h"
#include "ILogger.h"
#include "Application/Services/Platforms/IPlatform.h"

#include "Core/String/OpaaxUtf8.h"   // I7 — the one OpaaxString <-> fs::path conversion

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
        // NullPaths — every root empty, every resolver a safe no-op.
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

        // Stays an fs::path, never a narrow std::string: .string() would encode it through the ANSI
        // code page, and this name comes straight from the exe path (I7).
        const fs::path lAppName = lExe.stem();

        // Editor bakes the source workspace; release leaves it empty -> the exe dir.
        const fs::path lWorkspace = InWorkspaceDir.IsEmpty()
                                        ? lExeDir
                                        : Utf8::ToFsPath(InWorkspaceDir);

        // Project file: explicit --project (absolute kept, relative under the workspace),
        // else the default <workspace>/<AppName>/<AppName>.opaaxproj.
        fs::path lProjFile;
        if (!InProjectArg.IsEmpty())
        {
            const fs::path lArg = Utf8::ToFsPath(InProjectArg);
            lProjFile = lArg.is_absolute() ? lArg : (lWorkspace / lArg);
        }
        else
        {
            // Appending an ASCII literal is encoding-invariant, so it needs no conversion.
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
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
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

        // --project on the command line wins; else the host's declared project (editor host names the
        // game project); else empty -> ResolveProjectLayout uses the exe-stem default.
        OpaaxString lProjArg = FindProjectArg(InArgc, InArgv);
        if (lProjArg.IsEmpty())
        {
            lProjArg = InProjectOverride;
        }

        m_Layout = ResolveProjectLayout(lExe, lWorkspace, lProjArg);
    }

    void Paths::LogPaths() const
    {
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Workspace Path:      {}", WorkspaceRoot().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Engine Path:         {}", EngineRoot().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Project Root Path:   {}", ProjectRoot().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Project File Path:   {}", ProjectFile().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Assets Directory:    {}", AssetsDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Configs Directory:   {}", ConfigsDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Sources Directory:   {}", SourceDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Save Directory:      {}", SaveDir().CStr());
        OPAAX_APP_LOG(Info, "OpaaxApplication::Bootstrap ----> Temp Directory:      {}", TempDir().CStr());
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
        return Utf8::FromFsPath(Utf8::ToFsPath(m_Layout.AssetsDir) / Utf8::ToFsPath(InAssetRel));
    }

    OpaaxString Paths::AbsoluteToAsset(const OpaaxString& InAbsPath) const
    {
        if (InAbsPath.IsEmpty() || m_Layout.AssetsDir.IsEmpty())
        {
            return OpaaxString();
        }

        // weakly_canonical on BOTH sides, because the two arrive in different shapes: a file
        // dialog answers `C:\...\Maps\Main.opaaxmap` while AssetsDir was built with forward
        // slashes. Comparing the strings would say "outside the assets dir" for a file plainly
        // inside it. `weakly_` because the target need not exist yet (a Save As target).
        std::error_code lError;
        const fs::path lAbs  = fs::weakly_canonical(Utf8::ToFsPath(InAbsPath), lError);
        const fs::path lRoot = fs::weakly_canonical(Utf8::ToFsPath(m_Layout.AssetsDir), lError);

        if (lError)
        {
            return OpaaxString();
        }

        const fs::path lRelative = lAbs.lexically_relative(lRoot);

        // Empty means unrelated paths; a leading ".." means it climbed OUT of the assets dir. Both
        // are "this file cannot be named by a manifest", which is a real answer (see the header).
        if (lRelative.empty() || *lRelative.begin() == "..")
        {
            return OpaaxString();
        }

        return Utf8::FromFsPath(lRelative);
    }
}
