#include "Platform/DynamicLibrary.h"

#ifdef OPAAX_PLATFORM_POSIX

#include <dlfcn.h>

namespace Opaax
{
    namespace
    {
        /** dlerror's text, which also clears it. */
        OpaaxString TakeLoaderError()
        {
            const char* const lText = dlerror();
            return OpaaxString(lText != nullptr ? lText : "unknown error");
        }
    }

    bool DynamicLibrary::Open(const OpaaxString& InPath)
    {
        Close();
        m_Path = InPath;

        // Every symbol bound now: a library missing one fails here, not at its first call. Local, so
        // its symbols never stand in for the program's own.
        m_Handle = dlopen(InPath.CStr(), RTLD_NOW | RTLD_LOCAL);
        if (m_Handle == nullptr)
        {
            m_Error = TakeLoaderError();
            return false;
        }

        m_Error.Clear();
        return true;
    }

    void DynamicLibrary::Close() noexcept
    {
        if (m_Handle != nullptr)
        {
            dlclose(m_Handle);
            m_Handle = nullptr;
        }
    }

    void* DynamicLibrary::FindSymbol(const char* InName)
    {
        if (m_Handle == nullptr)
        {
            m_Error = "the library is not open";
            return nullptr;
        }

        // A symbol may be null by right: dlerror, cleared first, tells a missing one apart.
        dlerror();
        void* const lSymbol = dlsym(m_Handle, InName);
        if (lSymbol == nullptr)
        {
            m_Error = OpaaxString::Format("'{}' not found: {}", InName, TakeLoaderError().CStr());
            return nullptr;
        }

        m_Error.Clear();
        return lSymbol;
    }
}

#endif
