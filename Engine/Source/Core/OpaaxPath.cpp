#include "OpaaxPath.h"

namespace Opaax
{
    OpaaxString OpaaxPath::s_BasePath      = {};
    OpaaxString OpaaxPath::s_AppName       = {};
    OpaaxString OpaaxPath::s_WorkspaceRoot = {};
    OpaaxString OpaaxPath::s_EngineRoot    = {};
    OpaaxString OpaaxPath::s_ProjectRoot   = {};

    namespace
    {
        // In-place '\' -> '/' so every root and resolved path shares one separator convention.
        void NormalizeSeparators(OpaaxString& InOutPath)
        {
            for (Uint32 i = 0; i < InOutPath.GetLength(); ++i)
            {
                if (InOutPath.Data()[i] == '\\') { InOutPath.Data()[i] = '/'; }
            }
        }
    }

    bool OpaaxPath::IsAbsolute(const char* InPath) noexcept
    {
        if (!InPath || InPath[0] == '\0') { return false; }

        // Unix absolute: starts with /
        if (InPath[0] == '/') { return true; }

        // Windows absolute: drive letter e.g. C:/ or C:\\ (trailing backslash escaped to avoid C4010).
        if (InPath[1] == ':' && (InPath[2] == '/' || InPath[2] == '\\')) { return true; }

        return false;
    }

    OpaaxString OpaaxPath::ResolveAgainst(const OpaaxString& InBase, const char* InRel)
    {
        if (!InRel || InRel[0] == '\0')   { return InBase; }
        if (IsAbsolute(InRel))            { return OpaaxString(InRel); }

        OpaaxString lResult = InBase;
        lResult += "/";
        lResult += InRel;
        return lResult;
    }

    void OpaaxPath::Init()
    {
        // ----- Base path + app name — from the running executable --------------------
#if defined(OPAAX_PLATFORM_WINDOWS)
        wchar_t lBuffer[MAX_PATH];
        GetModuleFileNameW(nullptr, lBuffer, MAX_PATH);
        const std::filesystem::path lExePath(lBuffer);
        s_BasePath = OpaaxString(lExePath.parent_path().string().c_str());
        s_AppName  = OpaaxString(lExePath.stem().string().c_str());
#elif defined(OPAAX_BIN_DIR)
        s_BasePath = OpaaxString(OPAAX_BIN_DIR);
#else
        s_BasePath = OpaaxString(std::filesystem::current_path().string().c_str());
#endif
        NormalizeSeparators(s_BasePath);

        // ----- Workspace root — baked in editor builds, else the exe dir -------------
        // The workspace holds Engine/ + every game project. Release deploys mirror this
        // layout under the exe dir, so BasePath is the correct fallback there.
#if defined(OPAAX_WORKSPACE_DIR)
        s_WorkspaceRoot = OpaaxString(OPAAX_WORKSPACE_DIR);
        NormalizeSeparators(s_WorkspaceRoot);
#else
        s_WorkspaceRoot = s_BasePath;
#endif

        // ----- Engine + project roots are siblings under the workspace ---------------
        // AppName is the exe stem, which equals the project folder name by convention
        // (the CMake target name) — so the per-app project root needs no extra plumbing.
        s_EngineRoot = s_WorkspaceRoot;
        s_EngineRoot += "/Engine";

        s_ProjectRoot = s_WorkspaceRoot;
        s_ProjectRoot += "/";
        s_ProjectRoot += s_AppName;

        OPAAX_CORE_INFO("OpaaxPath::Init() — base path:      {}", s_BasePath);
        OPAAX_CORE_INFO("OpaaxPath::Init() — app:            {}", s_AppName);
        OPAAX_CORE_INFO("OpaaxPath::Init() — workspace root: {}", s_WorkspaceRoot);
        OPAAX_CORE_INFO("OpaaxPath::Init() — engine root:    {}", s_EngineRoot);
        OPAAX_CORE_INFO("OpaaxPath::Init() — project root:   {}", s_ProjectRoot);
    }

    OpaaxString OpaaxPath::ToAbsolute(const char* InRelativePath)        { return ResolveAgainst(s_WorkspaceRoot, InRelativePath); }
    OpaaxString OpaaxPath::ToAbsolute(const OpaaxString& InRelativePath) { return ResolveAgainst(s_WorkspaceRoot, InRelativePath.CStr()); }

    OpaaxString OpaaxPath::EngineToAbsolute(const char* InRelativePath)        { return ResolveAgainst(s_EngineRoot, InRelativePath); }
    OpaaxString OpaaxPath::EngineToAbsolute(const OpaaxString& InRelativePath) { return ResolveAgainst(s_EngineRoot, InRelativePath.CStr()); }

    OpaaxString OpaaxPath::ProjectToAbsolute(const char* InRelativePath)        { return ResolveAgainst(s_ProjectRoot, InRelativePath); }
    OpaaxString OpaaxPath::ProjectToAbsolute(const OpaaxString& InRelativePath) { return ResolveAgainst(s_ProjectRoot, InRelativePath.CStr()); }

    OpaaxString OpaaxPath::ToProjectRelative(const char* InAbsPath) noexcept
    {
        if (!InAbsPath || InAbsPath[0] == '\0') { return OpaaxString(); }

        const OpaaxString lAbs(InAbsPath);
        const OpaaxString lAbsLower = lAbs.ToLower();

        // Manifest paths are workspace-relative; BasePath is the release fallback.
        const OpaaxString* lRoots[] = { &s_WorkspaceRoot, &s_BasePath };
        for (const OpaaxString* lRoot : lRoots)
        {
            if (lRoot->IsEmpty()) { continue; }

            if (lAbsLower.Find(lRoot->ToLower().CStr()) == 0)
            {
                const Uint32 lSkip = lRoot->GetLength() + 1; // +1 for the '/' separator
                if (lSkip >= lAbs.GetLength()) { return OpaaxString(); }
                return lAbs.SubString(lSkip);
            }
        }

        // Outside every known root — return unchanged. Handles system / external paths.
        OPAAX_CORE_WARN("OpaaxPath::ToProjectRelative — '{}' is not under the workspace root", InAbsPath);
        return lAbs;
    }

    OpaaxString OpaaxPath::ToProjectRelative(const OpaaxString& InAbsPath) noexcept
    {
        return ToProjectRelative(InAbsPath.CStr());
    }

    bool OpaaxPath::IsAbsolutePath(const OpaaxString& InPath) noexcept
    {
        return IsAbsolute(InPath.CStr());
    }
}
