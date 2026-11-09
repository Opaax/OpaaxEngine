#include "Platform/DynamicLibrary.h"

#ifdef OPAAX_PLATFORM_WINDOWS

#include "Core/String/OpaaxUtf8.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace Opaax
{
    namespace
    {
        /** The system's message for InCode, without its trailing line break. */
        OpaaxString SystemMessage(const DWORD InCode)
        {
            wchar_t* lBuffer = nullptr;
            const DWORD lLength = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                                 FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, InCode, 0,
                                                 reinterpret_cast<wchar_t*>(&lBuffer), 0, nullptr);
            OpaaxString lMessage;
            if (lLength > 0 && lBuffer != nullptr)
            {
                DWORD lEnd = lLength;
                while (lEnd > 0 && (lBuffer[lEnd - 1] == L'\r' || lBuffer[lEnd - 1] == L'\n' || lBuffer[lEnd - 1] == L' '))
                {
                    --lEnd;
                }
                lMessage = Utf8::FromWide({ lBuffer, lEnd });
            }
            LocalFree(lBuffer);

            OpaaxString lText = OpaaxString::Format("error {}", static_cast<Uint32>(InCode));
            return lMessage.IsEmpty() ? lText : lMessage + " (" + lText + ")";
        }

        bool HasFolder(const OpaaxString& InPath)
        {
            return InPath.Find('/') >= 0 || InPath.Find('\\') >= 0;
        }
    }

    bool DynamicLibrary::Open(const OpaaxString& InPath)
    {
        Close();
        m_Path = InPath;

        // Backslashes: LoadLibraryEx refuses a path with forward slashes for the search flags below.
        std::wstring lWide = Utf8::ToWide(InPath);
        for (wchar_t& lChar : lWide)
        {
            if (lChar == L'/') { lChar = L'\\'; }
        }

        // A library given with its folder finds its own dependencies beside it; a bare name is searched
        // beside the executable and in System32. Neither looks in the working directory or PATH, where
        // a planted file would be loaded instead.
        const DWORD lFlags = HasFolder(InPath) ? (LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)
                                               : LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        m_Handle = LoadLibraryExW(lWide.c_str(), nullptr, lFlags);
        if (m_Handle == nullptr)
        {
            m_Error = SystemMessage(GetLastError());
            return false;
        }

        m_Error.Clear();
        return true;
    }

    void DynamicLibrary::Close() noexcept
    {
        if (m_Handle != nullptr)
        {
            FreeLibrary(static_cast<HMODULE>(m_Handle));
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

        void* const lSymbol = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(m_Handle), InName));
        if (lSymbol == nullptr)
        {
            m_Error = OpaaxString::Format("'{}' not found: {}", InName, SystemMessage(GetLastError()).CStr());
            return nullptr;
        }

        m_Error.Clear();
        return lSymbol;
    }
}

#endif
