#pragma once

#include "Core/String/OpaaxString.hpp"

#ifdef OPAAX_PLATFORM_WINDOWS

#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace Opaax::Windows
{
    // =============================================================================
    // The UTF-8 <-> UTF-16 boundary, in one place.
    //
    // Every string the engine carries is UTF-8; every Windows call that matters takes UTF-16. The two
    // narrow-char shortcuts both LOOK correct and are not: the *A Win32 entry points and MSVC's
    // std::filesystem::path(const char*) each decode using the ANSI code page, so a non-ASCII path
    // resolves to a different file than the one the caller named — with no error anywhere.
    //
    // LENIENT ON PURPOSE (no MB_ERR_INVALID_CHARS): malformed input degrades to U+FFFD rather than
    // failing the conversion, so the path simply does not exist and the caller gets an ordinary false.
    // Nothing here throws.
    // =============================================================================

    /** UTF-8 -> UTF-16. Empty in, empty out. */
    inline std::wstring Utf8ToWide(const OpaaxString& InUtf8)
    {
        if (InUtf8.IsEmpty())
        {
            return {};
        }

        const int lUtf8Len = static_cast<int>(InUtf8.GetLength());
        const int lWideLen = MultiByteToWideChar(CP_UTF8, 0, InUtf8.CStr(), lUtf8Len, nullptr, 0);
        if (lWideLen <= 0)
        {
            return {};
        }

        std::wstring lWide(static_cast<size_t>(lWideLen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, InUtf8.CStr(), lUtf8Len, lWide.data(), lWideLen);
        return lWide;
    }

    /** UTF-16 -> UTF-8. Empty in, empty out. */
    inline OpaaxString WideToUtf8(const std::wstring& InWide)
    {
        if (InWide.empty())
        {
            return {};
        }

        const int lWideLen = static_cast<int>(InWide.size());
        const int lUtf8Len = WideCharToMultiByte(CP_UTF8, 0, InWide.data(), lWideLen,
                                                 nullptr, 0, nullptr, nullptr);
        if (lUtf8Len <= 0)
        {
            return {};
        }

        std::string lUtf8(static_cast<size_t>(lUtf8Len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, InWide.data(), lWideLen,
                            lUtf8.data(), lUtf8Len, nullptr, nullptr);
        return OpaaxString(lUtf8.c_str());
    }
}

#endif // OPAAX_PLATFORM_WINDOWS
