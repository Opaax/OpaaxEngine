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

    // =============================================================================
    // Codepoints — reading UTF-8 as CHARACTERS rather than as a path.
    //
    // The other half of the boundary, and the one text rendering needs: a glyph is keyed by
    // codepoint, so "Γειά" has to become four numbers rather than eight bytes. Same file because it
    // is the same invariant (**I7**) seen from the other end, and the same no-throw contract.
    // =============================================================================

    /** What malformed input decodes to — U+FFFD REPLACEMENT CHARACTER, the Unicode-sanctioned answer. */
    inline constexpr Uint32 REPLACEMENT = 0xFFFDu;

    /**
     * Decode the codepoint at InOutCursor and advance past it.
     *
     * ALWAYS ADVANCES on a non-empty string — that is the contract that matters, because every caller
     * is a `while` loop and a decoder that can stand still turns one bad byte into a hang. Malformed
     * input (a stray continuation byte, a truncated sequence, an overlong form, a surrogate half)
     * consumes ONE byte and answers REPLACEMENT, so the rest of the string still reads.
     *
     * @param InOutCursor Cursor into a NUL-terminated UTF-8 string. Advanced past the codepoint read.
     * @return The codepoint, or 0 at the terminator — the loop condition, and the cursor stays put.
     */
    inline Uint32 Decode(const char*& InOutCursor) noexcept
    {
        const Uint8* lBytes = reinterpret_cast<const Uint8*>(InOutCursor);
        const Uint8  lLead  = lBytes[0];

        if (lLead == 0u)
        {
            return 0u;   // the terminator: answer "done" WITHOUT advancing past it
        }

        // How many bytes the lead announces. 0xC0/0xC1 are overlong two-byte forms and 0xF5+ is past
        // U+10FFFF, so both are rejected by the lead alone rather than after decoding.
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

            // Catches the truncated sequence too: a NUL is not 10xxxxxx, so a lead byte at the end of
            // the string cannot walk the cursor past the terminator.
            if ((lContinuation & 0xC0u) != 0x80u)
            {
                ++InOutCursor;
                return REPLACEMENT;
            }

            lCodepoint = (lCodepoint << 6) | (lContinuation & 0x3Fu);
        }

        // Overlong three/four-byte forms and the UTF-16 surrogate range. Both are encodable and both
        // are illegal — a decoder that passes them on hands the caller a codepoint no font has.
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
