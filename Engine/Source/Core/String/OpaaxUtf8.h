#pragma once

#include "Core/String/OpaaxString.hpp"

#include <filesystem>
#include <string>

#ifdef OPAAX_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

namespace Opaax::Utf8
{
    // =============================================================================
    // The UTF-8 boundary, in ONE place (ARCHITECTURE.md I7).
    //
    // Every string the engine carries is UTF-8 — the platform layer fixed that by converting
    // GetExecutablePath through CP_UTF8. The OS does not agree by default, and the two shortcuts that
    // look right are the ones that are not: on MSVC, std::filesystem::path(const char*) and
    // fstream(const char*) both decode using the ANSI code page, so a non-ASCII path silently resolves
    // to a DIFFERENT FILE than the caller named — no exception, no error code, no signal.
    //
    // So: never build an fs::path or open a stream from OpaaxString::CStr(). Go through ToFsPath.
    // std::fstream takes an fs::path since C++17, which makes every fix a one-line swap.
    //
    // This lives in Core because the layers that need it (Core config IO, the portable Renderer) sit
    // BELOW Application and cannot reach IPlatform's filesystem. IFileSystem is one consumer of this
    // rule, not the only legal way to touch a file.
    //
    // Header-only and stateless — no OPAAX_API (I6). Nothing here throws; malformed input degrades to
    // U+FFFD, so the path simply does not exist and the caller gets its ordinary failure.
    // =============================================================================

#ifdef OPAAX_PLATFORM_WINDOWS

    /** UTF-8 -> UTF-16. Empty in, empty out. */
    inline std::wstring ToWide(const OpaaxString& InUtf8)
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
    inline OpaaxString FromWide(const std::wstring& InWide)
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

    /** UTF-8 -> fs::path. Goes through UTF-16 so the ANSI code page never sees it. */
    inline std::filesystem::path ToFsPath(const OpaaxString& InUtf8)
    {
        return std::filesystem::path(ToWide(InUtf8));
    }

    /** fs::path -> UTF-8, forward slashes (the engine's path convention). */
    inline OpaaxString FromFsPath(const std::filesystem::path& InPath)
    {
        return FromWide(InPath.generic_wstring());
    }

#else

    // POSIX: the native narrow encoding IS UTF-8, so both directions are already correct and the
    // conversion collapses. Kept as the same two names so no call site is platform-aware.

    inline std::filesystem::path ToFsPath(const OpaaxString& InUtf8)
    {
        return std::filesystem::path(InUtf8.CStr());
    }

    inline OpaaxString FromFsPath(const std::filesystem::path& InPath)
    {
        return OpaaxString(InPath.generic_string().c_str());
    }

#endif // OPAAX_PLATFORM_WINDOWS
}
