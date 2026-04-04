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
        // std::filesystem::current_path() is the working directory — unreliable.
        // We don't use it.
        //
        // On Windows, the exe path comes from the process image path.
        // std::filesystem doesn't expose this directly, but we can use
        // argv[0] captured at main() entry, or the platform API.
        // We use the CMake-injected compile definition as the fallback for
        // cases where the exe path API isn't available.
        //
        // NOTE: We try three sources in priority order:
        //   1. Platform API (most reliable — actual exe location)
        //   2. CMAKE_BINARY_DIR compile definition (reliable in dev builds)
        //   3. Working directory (last resort, may be wrong)
 
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
}
