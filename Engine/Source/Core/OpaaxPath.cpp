#include "OpaaxPath.h"

namespace Opaax
{
    Opaax::OpaaxString OpaaxPath::s_BasePath = {};
    
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
 
        /**
         * Windows absolute: starts with drive letter e.g. C:/  or C:\
         */
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
    }

    OpaaxString OpaaxPath::Resolve(const char* InRelativePath)
    {
        if (!InRelativePath || InRelativePath[0] == '\0')
        {
            return s_BasePath;
        }
 
        // Already absolute — pass through
        if (IsAbsolute(InRelativePath))
        {
            return OpaaxString(InRelativePath);
        }
 
        // Combine base + relative
        OpaaxString lResult = s_BasePath;
        lResult += "/";
        lResult += InRelativePath;
        return lResult;
    }

    OpaaxString OpaaxPath::Resolve(const OpaaxString& InRelativePath)
    {
        return Resolve(InRelativePath.CStr());
    }

    OpaaxString OpaaxPath::MakeRelative(const char* InAbsPath) noexcept
    {
        if (!InAbsPath || InAbsPath[0] == '\0')
        {
            return OpaaxString();
        }

        const OpaaxString lAbs(InAbsPath);
        const OpaaxString& lBase = s_BasePath;
        
        const OpaaxString lAbsLower  = lAbs.ToLower();
        const OpaaxString lBaseLower = lBase.ToLower();

        if (lAbsLower.Find(lBaseLower.CStr()) != 0)
        {
            // Does not start with base path — return unchanged.
            // This handles absolute paths outside the project (system assets, etc.)
            OPAAX_CORE_WARN("OpaaxPath::MakeRelative — '{}' is not under base path '{}'", InAbsPath, lBase);
            return lAbs;
        }

        // Skip base path + trailing separator
        const Uint32 lSkip = lBase.GetLength() + 1; // +1 for '/'
        if (lSkip >= lAbs.GetLength())
        {
            return OpaaxString();
        }

        return lAbs.SubString(lSkip);
    }

    OpaaxString OpaaxPath::MakeRelative(const OpaaxString& InAbsPath) noexcept
    {
        return MakeRelative(InAbsPath.CStr());
    }

    bool OpaaxPath::IsAbsolutePath(const OpaaxString& InPath) noexcept
    {
        return IsAbsolute(InPath.CStr());
    }
}
