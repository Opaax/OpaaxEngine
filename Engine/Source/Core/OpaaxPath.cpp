#include "OpaaxPath.h"

namespace Opaax
{
    Opaax::OpaaxString OpaaxPath::s_BasePath    = {};
    Opaax::OpaaxString OpaaxPath::s_ProjectRoot = {};

    bool OpaaxPath::IsAbsolute(const char* InPath) noexcept
    {
        if (!InPath || InPath[0] == '\0')
        {
            return false;
        }

        // Unix absolute: starts with /
        if (InPath[0] == '/')
        {
            return true;
        }

        // Windows absolute: drive letter e.g. C:/ or C:\\ (trailing backslash escaped to avoid C4010 line-continuation)
        if (InPath[1] == ':' && (InPath[2] == '/' || InPath[2] == '\\'))
        {
            return true;
        }

        return false;
    }

    void OpaaxPath::Init()
    {
#if defined(OPAAX_PLATFORM_WINDOWS)
        wchar_t lBuffer[MAX_PATH];
        GetModuleFileNameW(nullptr, lBuffer, MAX_PATH);
        const std::filesystem::path lExePath = std::filesystem::path(lBuffer).parent_path();
        s_BasePath = OpaaxString(lExePath.string().c_str());
#elif defined(OPAAX_BIN_DIR)
        s_BasePath = OpaaxString(OPAAX_BIN_DIR);
#else
        s_BasePath = OpaaxString(std::filesystem::current_path().string().c_str());
#endif
        // Normalise separators
        for (Uint32 i = 0; i < s_BasePath.GetLength(); ++i)
        {
            if (s_BasePath.Data()[i] == '\\') { s_BasePath.Data()[i] = '/'; }
        }

        OPAAX_CORE_INFO("OpaaxPath::Init() — base path: {}", s_BasePath);

#if defined(OPAAX_PROJECT_ROOT)
        s_ProjectRoot = OpaaxString(OPAAX_PROJECT_ROOT);
        for (Uint32 i = 0; i < s_ProjectRoot.GetLength(); ++i)
        {
            if (s_ProjectRoot.Data()[i] == '\\') { s_ProjectRoot.Data()[i] = '/'; }
        }
        OPAAX_CORE_INFO("OpaaxPath::Init() — project root: {}", s_ProjectRoot);
#endif
    }

    OpaaxString OpaaxPath::ToAbsolute(const char* InRelativePath)
    {
        // NOTE: Project root is the canonical base in editor builds; base path
        // (exe dir) is the fallback for release builds where OPAAX_PROJECT_ROOT
        // is undefined. CMake deploys under the same relative names so the same
        // input string resolves correctly in both modes.
        const OpaaxString& lBase = !s_ProjectRoot.IsEmpty() ? s_ProjectRoot : s_BasePath;

        if (!InRelativePath || InRelativePath[0] == '\0')
        {
            return lBase;
        }

        if (IsAbsolute(InRelativePath))
        {
            return OpaaxString(InRelativePath);
        }

        OpaaxString lResult = lBase;
        lResult += "/";
        lResult += InRelativePath;
        return lResult;
    }

    OpaaxString OpaaxPath::ToAbsolute(const OpaaxString& InRelativePath)
    {
        return ToAbsolute(InRelativePath.CStr());
    }

    OpaaxString OpaaxPath::ToProjectRelative(const char* InAbsPath) noexcept
    {
        if (!InAbsPath || InAbsPath[0] == '\0')
        {
            return OpaaxString();
        }

        const OpaaxString lAbs(InAbsPath);
        // NOTE: Match against project root first (editor); fall back to base path
        // (release) so the function still produces something useful when no
        // project root is baked in.
        const OpaaxString& lBase = !s_ProjectRoot.IsEmpty() ? s_ProjectRoot : s_BasePath;

        const OpaaxString lAbsLower  = lAbs.ToLower();
        const OpaaxString lBaseLower = lBase.ToLower();

        if (lAbsLower.Find(lBaseLower.CStr()) != 0)
        {
            // Outside the known roots — return unchanged. Handles system / external paths.
            OPAAX_CORE_WARN("OpaaxPath::ToProjectRelative — '{}' is not under '{}'", InAbsPath, lBase);
            return lAbs;
        }

        // Skip base + trailing separator
        const Uint32 lSkip = lBase.GetLength() + 1; // +1 for '/'
        if (lSkip >= lAbs.GetLength())
        {
            return OpaaxString();
        }

        return lAbs.SubString(lSkip);
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
