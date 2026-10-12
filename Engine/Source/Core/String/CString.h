#pragma once

#include <cstdlib>
#include <cstring>

#include <spdlog/fmt/fmt.h>

#include "Core/OpaaxTypes.h"

// =============================================================================
// CString — functions on null-terminated char strings: the edges where text is a raw buffer (an
//   ImGui label, argv, a vendor's name). Engine text is an OpaaxString or an OpaaxStringView.
// =============================================================================
namespace Opaax::CString
{
    /** Bytes before the terminator. */
    inline Uint64 Length(const char* InText) noexcept
    {
        return static_cast<Uint64>(std::strlen(InText));
    }

    /** @return Below 0, 0 or above 0 as InA sorts before, equal to or after InB (byte order) */
    inline Int32 Compare(const char* InA, const char* InB) noexcept
    {
        return std::strcmp(InA, InB);
    }

    inline bool Equals(const char* InA, const char* InB) noexcept
    {
        return std::strcmp(InA, InB) == 0;
    }

    /** @return The first InNeedle in InText, or nullptr */
    inline const char* Find(const char* InText, const char* InNeedle) noexcept
    {
        return std::strstr(InText, InNeedle);
    }

    /**
     * Reads an integer at the start of InText: spaces are skipped, a sign is allowed (strtoll).
     * @param OutEnd Receives where the number stopped (InText when there was none)
     * @return The number, or 0 when there is none
     */
    inline Int64 ToInt64(const char* InText, const char** OutEnd = nullptr, const Int32 InBase = 10) noexcept
    {
        char*       lEnd   = nullptr;
        const Int64 lValue = static_cast<Int64>(std::strtoll(InText, &lEnd, InBase));
        if (OutEnd != nullptr) { *OutEnd = lEnd; }
        return lValue;
    }

    /** ToInt64 for an unsigned number (strtoull). */
    inline Uint64 ToUint64(const char* InText, const char** OutEnd = nullptr, const Int32 InBase = 10) noexcept
    {
        char*        lEnd   = nullptr;
        const Uint64 lValue = static_cast<Uint64>(std::strtoull(InText, &lEnd, InBase));
        if (OutEnd != nullptr) { *OutEnd = lEnd; }
        return lValue;
    }

    /** Reads a decimal number at the start of InText (strtod): "1.5", "-2e3"... 0 when there is none. */
    inline double ToDouble(const char* InText, const char** OutEnd = nullptr) noexcept
    {
        char*        lEnd   = nullptr;
        const double lValue = std::strtod(InText, &lEnd);
        if (OutEnd != nullptr) { *OutEnd = lEnd; }
        return lValue;
    }

    /**
     * Writes text formatted like OPAAX_LOG ("{}" fields, checked at compile time) into OutBuffer,
     * cut to fit, always terminated.
     * @return The length written
     */
    template<size_t N, typename... TArgs>
    Uint64 Format(char (&OutBuffer)[N], fmt::format_string<TArgs...> InFormat, TArgs&&... InArgs)
    {
        static_assert(N > 0, "CString::Format needs room for the terminator");

        const auto lResult = fmt::format_to_n(OutBuffer, N - 1, InFormat, Forward<TArgs>(InArgs)...);
        *lResult.out = '\0';
        return static_cast<Uint64>(lResult.out - OutBuffer);
    }
}
