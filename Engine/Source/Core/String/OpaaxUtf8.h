#pragma once

#include "Core/OpaaxTypes.h"
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
    // UTF-8 helpers. Every engine string is UTF-8.
    //
    // Never build an fs::path or open a stream from OpaaxString::CStr(): on MSVC it uses the
    // ANSI code page and opens a different file for non-ASCII paths. Use ToFsPath.
    // Never throws; malformed input becomes U+FFFD.
    // =============================================================================

    // =============================================================================
    // Codepoints — decode UTF-8 text into characters (for text rendering).
    // =============================================================================

    /** Returned for malformed input: U+FFFD. */
    inline constexpr Uint32 REPLACEMENT = 0xFFFDu;

    /**
     * Decodes the codepoint at InOutCursor and moves past it.
     * Always advances on a non-empty string; malformed input consumes one byte and returns REPLACEMENT.
     * @param InOutCursor Cursor into a null-terminated UTF-8 string
     * @return The codepoint, or 0 at the terminator (the cursor does not move)
     */
    inline Uint32 Decode(const char*& InOutCursor) noexcept
    {
        const Uint8* lBytes = reinterpret_cast<const Uint8*>(InOutCursor);
        const Uint8  lLead  = lBytes[0];

        if (lLead == 0u)
        {
            return 0u;   // terminator: do not advance
        }

        // Byte count from the lead byte. 0xC0/0xC1 (overlong) and 0xF5+ (past U+10FFFF) are rejected.
        Uint32 lLength   = 0u;
        Uint32 lCodepoint = 0u;

        if (lLead < 0x80u)                        { lLength = 1u; lCodepoint = lLead; }
        else if (lLead >= 0xC2u && lLead <= 0xDFu) { lLength = 2u; lCodepoint = lLead & 0x1Fu; }
        else if (lLead >= 0xE0u && lLead <= 0xEFu) { lLength = 3u; lCodepoint = lLead & 0x0Fu; }
        else if (lLead >= 0xF0u && lLead <= 0xF4u) { lLength = 4u; lCodepoint = lLead & 0x07u; }
        else
        {
            ++InOutCursor;
            return REPLACEMENT;
        }

        for (Uint32 lIndex = 1u; lIndex < lLength; ++lIndex)
        {
            const Uint8 lContinuation = lBytes[lIndex];

            // Also catches a truncated sequence (the terminator is not a continuation byte).
            if ((lContinuation & 0xC0u) != 0x80u)
            {
                ++InOutCursor;
                return REPLACEMENT;
            }

            lCodepoint = (lCodepoint << 6) | (lContinuation & 0x3Fu);
        }

        // Reject overlong forms and UTF-16 surrogates.
        const bool bOverlong  = (lLength == 3u && lCodepoint < 0x800u)
                             || (lLength == 4u && lCodepoint < 0x10000u);
        const bool bSurrogate = (lCodepoint >= 0xD800u && lCodepoint <= 0xDFFFu);

        if (bOverlong || bSurrogate || lCodepoint > 0x10FFFFu)
        {
            ++InOutCursor;
            return REPLACEMENT;
        }

        InOutCursor += lLength;
        return lCodepoint;
    }

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

    /** UTF-8 -> fs::path (through UTF-16). */
    inline std::filesystem::path ToFsPath(const OpaaxString& InUtf8)
    {
        return std::filesystem::path(ToWide(InUtf8));
    }

    /** fs::path -> UTF-8, with forward slashes. */
    inline OpaaxString FromFsPath(const std::filesystem::path& InPath)
    {
        return FromWide(InPath.generic_wstring());
    }

#else

    // POSIX: the native encoding is already UTF-8.

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
