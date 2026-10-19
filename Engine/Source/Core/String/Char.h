#pragma once

// =============================================================================
// Char — ASCII character classes and case, the same in every locale (the "C" locale's isspace,
//   isalnum, tolower...). A byte of a UTF-8 sequence is never a letter, a digit or a space.
// =============================================================================
namespace Opaax::Char
{
    /** Space, tab, line feed, vertical tab, form feed, carriage return. */
    constexpr bool IsSpace(const char InChar) noexcept
    {
        return InChar == ' ' || (InChar >= '\t' && InChar <= '\r');
    }

    constexpr bool IsDigit(const char InChar) noexcept
    {
        return InChar >= '0' && InChar <= '9';
    }

    constexpr bool IsAlpha(const char InChar) noexcept
    {
        return (InChar >= 'a' && InChar <= 'z') || (InChar >= 'A' && InChar <= 'Z');
    }

    constexpr bool IsAlnum(const char InChar) noexcept
    {
        return IsAlpha(InChar) || IsDigit(InChar);
    }

    constexpr char ToLower(const char InChar) noexcept
    {
        return (InChar >= 'A' && InChar <= 'Z') ? static_cast<char>(InChar - 'A' + 'a') : InChar;
    }

    constexpr char ToUpper(const char InChar) noexcept
    {
        return (InChar >= 'a' && InChar <= 'z') ? static_cast<char>(InChar - 'a' + 'A') : InChar;
    }
}
