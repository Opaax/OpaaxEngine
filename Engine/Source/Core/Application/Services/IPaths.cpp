#include "IPaths.h"
#include "IPlatform.h"

#include <cstring>
#include <filesystem>

namespace Opaax
{
    namespace
    {
        namespace fs = std::filesystem;

        // std::filesystem yields '/' via generic_string() on every OS — matches the
        // engine's normalised path convention.
        OpaaxString ToOpaax(const fs::path& InPath)
        {
            return OpaaxString(InPath.generic_string().c_str());
        }

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
        };
    }

    // =========================================================================
    // Pure resolver
    // =========================================================================
    ProjectLayout ResolveProjectLayout(const OpaaxString& InExePath,
                                       const OpaaxString& InWorkspaceDir,
                                       const OpaaxString& InProjectArg)
    {
        const fs::path    lExe(InExePath.CStr());
        const fs::path    lExeDir  = lExe.parent_path();
        const std::string lAppName = lExe.stem().string();

        // Editor bakes the source workspace; release leaves it empty -> the exe dir.
        const fs::path lWorkspace = InWorkspaceDir.IsEmpty()
                                        ? lExeDir
                                        : fs::path(InWorkspaceDir.CStr());

        // Project file: explicit --project (absolute kept, relative under the workspace),
        // else the default <workspace>/<AppName>/<AppName>.opaaxproj.
        fs::path lProjFile;
        if (!InProjectArg.IsEmpty())
        {
            const fs::path lArg(InProjectArg.CStr());
            lProjFile = lArg.is_absolute() ? lArg : (lWorkspace / lArg);
        }
        else
        {
            lProjFile = lWorkspace / lAppName / (lAppName + ".opaaxproj");
        }
        lProjFile = lProjFile.lexically_normal();

        const fs::path lProjRoot = lProjFile.parent_path();

        ProjectLayout lOut;
        lOut.WorkspaceRoot = ToOpaax(lWorkspace);
        lOut.EngineRoot    = ToOpaax(lWorkspace / "Engine");
        lOut.ProjectRoot   = ToOpaax(lProjRoot);
        lOut.ProjectFile   = ToOpaax(lProjFile);
        lOut.AssetsDir     = ToOpaax(lProjRoot / "Assets");
        lOut.ConfigsDir    = ToOpaax(lProjRoot / "Configs");
        lOut.SourceDir     = ToOpaax(lProjRoot / "Source");
        lOut.SaveDir       = ToOpaax(lProjRoot / "Save");
        lOut.TempDir       = ToOpaax(lProjRoot / "Temp");
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
    Paths::Paths(const IPlatform& InPlatform, int InArgc, char** InArgv)
    {
        OpaaxString lWorkspace;
#if OPAAX_WITH_EDITOR
        lWorkspace = OpaaxString(OPAAX_WORKSPACE_DIR);
#endif
        const OpaaxString lExe     = InPlatform.GetExecutablePath();
        const OpaaxString lProjArg = FindProjectArg(InArgc, InArgv);
        m_Layout = ResolveProjectLayout(lExe, lWorkspace, lProjArg);
    }

    OpaaxString Paths::EngineToAbsolute(const OpaaxString& InEngineRel) const
    {
        const fs::path lRoot(m_Layout.EngineRoot.CStr());
        return ToOpaax(lRoot / InEngineRel.CStr());
    }

    OpaaxString Paths::ProjectToAbsolute(const OpaaxString& InProjectRel) const
    {
        const fs::path lRoot(m_Layout.ProjectRoot.CStr());
        return ToOpaax(lRoot / InProjectRel.CStr());
    }

    OpaaxString Paths::AssetToAbsolute(const OpaaxString& InAssetRel) const
    {
        const fs::path lRoot(m_Layout.AssetsDir.CStr());
        return ToOpaax(lRoot / InAssetRel.CStr());
    }
}
