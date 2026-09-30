#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class OpaaxString;

    /**
     * Non-owning view over bytes: {const char*, Uint32}. Pass by value.
     * Not null-terminated (no CStr()); use ToString() when a terminator is needed.
     * Like std::string_view, do not keep it longer than the bytes it points to.
     *
     * @see OpaaxString for the owning form.
     * @see Core/Hash/OpaaxHash.h for std::hash<OpaaxStringView>.
     */
    class OpaaxStringView final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        constexpr OpaaxStringView() noexcept = default;

        /** Null-terminated bytes. nullptr gives an empty view. */
        constexpr OpaaxStringView(const char* InStr) noexcept
            : m_Data(InStr)
            , m_Length(InStr ? static_cast<Uint32>(std::char_traits<char>::length(InStr)) : 0) {}

        /** InStr need not be null-terminated; nothing past InLength is read. */
        constexpr OpaaxStringView(const char* InStr, Uint32 InLength) noexcept
            : m_Data(InStr), m_Length(InStr ? InLength : 0) {}

        // Implicit: conversions for third-party APIs (entt, nlohmann, fmt).
        constexpr OpaaxStringView(std::string_view InView) noexcept
            : m_Data(InView.data()), m_Length(static_cast<Uint32>(InView.size())) {}

        OpaaxStringView(const std::string& InStr) noexcept
            : m_Data(InStr.data()), m_Length(static_cast<Uint32>(InStr.size())) {}

        // OpaaxString converts through its own operator OpaaxStringView().

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // Single place for comparisons: a zero-length compare never dereferences.
        constexpr bool Matches(Uint32 InAt, OpaaxStringView InOther) const noexcept
        {
            return InOther.m_Length == 0
                       || std::char_traits<char>::compare(m_Data + InAt, InOther.m_Data, InOther.m_Length) == 0;
        }

    public:
        /** A null-terminated copy. Defined in OpaaxString.hpp. */
        OpaaxString ToString() const;

        /**
         * A sub-view (no allocation).
         * @return Empty if InStart is past the end
         */
        constexpr OpaaxStringView SubString(Uint32 InStart, Uint32 InLength = UINT32_MAX) const noexcept
        {
            if (InStart >= m_Length) { return OpaaxStringView(); }

            // Clamp against the remainder (InStart + InLength can overflow).
            const Uint32 lRemaining = m_Length - InStart;
            const Uint32 lActual    = (InLength > lRemaining) ? lRemaining : InLength;

            return OpaaxStringView(m_Data + InStart, lActual);
        }

        /** Narrows the view in place. Counts past the end are clamped. */
        constexpr void RemovePrefix(Uint32 InCount) noexcept
        {
            const Uint32 lActual = (InCount > m_Length) ? m_Length : InCount;
            m_Data += lActual;
            m_Length -= lActual;
        }

        constexpr void RemoveSuffix(Uint32 InCount) noexcept
        {
            m_Length -= (InCount > m_Length) ? m_Length : InCount;
        }

        /**
         * @return Index of the first match at or after InStartPos, or -1.
         *   An empty needle returns InStartPos.
         */
        constexpr Int32 Find(OpaaxStringView InNeedle, Uint32 InStartPos = 0) const noexcept
        {
            if (InStartPos > m_Length) { return -1; }
            if (InNeedle.m_Length == 0) { return static_cast<Int32>(InStartPos); }
            if (InNeedle.m_Length > m_Length - InStartPos) { return -1; }

            const Uint32 lLast = m_Length - InNeedle.m_Length;
            for (Uint32 i = InStartPos; i <= lLast; ++i)
            {
                if (Matches(i, InNeedle)) { return static_cast<Int32>(i); }
            }
            return -1;
        }

        constexpr Int32 Find(char InChar, Uint32 InStartPos = 0) const noexcept
        {
            for (Uint32 i = InStartPos; i < m_Length; ++i)
            {
                if (m_Data[i] == InChar) { return static_cast<Int32>(i); }
            }
            return -1;
        }

        /** @return Index of the last match, or -1. An empty needle returns the length. */
        constexpr Int32 FindLast(OpaaxStringView InNeedle) const noexcept
        {
            if (InNeedle.m_Length == 0) { return static_cast<Int32>(m_Length); }
            if (InNeedle.m_Length > m_Length) { return -1; }

            for (Uint32 i = m_Length - InNeedle.m_Length + 1; i > 0; --i)
            {
                if (Matches(i - 1, InNeedle)) { return static_cast<Int32>(i - 1); }
            }
            return -1;
        }

        constexpr Int32 FindLast(char InChar) const noexcept
        {
            for (Uint32 i = m_Length; i > 0; --i)
            {
                if (m_Data[i - 1] == InChar) { return static_cast<Int32>(i - 1); }
            }
            return -1;
        }

        /** @return Index of the last byte that is any of InChars, or -1. */
        constexpr Int32 FindLastOf(OpaaxStringView InChars) const noexcept
        {
            for (Uint32 i = m_Length; i > 0; --i)
            {
                if (InChars.Find(m_Data[i - 1]) >= 0) { return static_cast<Int32>(i - 1); }
            }
            return -1;
        }

        constexpr bool StartsWith(OpaaxStringView InPrefix) const noexcept
        {
            return InPrefix.m_Length <= m_Length && Matches(0, InPrefix);
        }

        constexpr bool EndsWith(OpaaxStringView InSuffix) const noexcept
        {
            return InSuffix.m_Length <= m_Length && Matches(m_Length - InSuffix.m_Length, InSuffix);
        }

        constexpr bool Contains(OpaaxStringView InNeedle) const noexcept { return Find(InNeedle) >= 0; }

        //----------------------------------------------------------------------------------------
        //Get - Set

        /** The bytes. Not null-terminated. */
        constexpr const char* Data() const noexcept { return m_Data; }

        constexpr Uint32 GetLength() const noexcept { return m_Length; }
        constexpr bool   IsEmpty()   const noexcept { return m_Length == 0; }

        constexpr bool IsValidIndex(Uint32 InIndex) const noexcept { return InIndex < m_Length; }

        constexpr const char* begin() const noexcept { return m_Data; }
        constexpr const char* end()   const noexcept { return m_Data + m_Length; }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        constexpr char operator[](Uint32 InIndex) const noexcept
        {
            return IsValidIndex(InIndex) ? m_Data[InIndex] : '\0';
        }

        /** Conversion to std::string_view. */
        constexpr operator std::string_view() const noexcept
        {
            return m_Data ? std::string_view(m_Data, m_Length) : std::string_view();
        }

        // Literals, const char*, OpaaxString and std::string_view all convert to a view.
        constexpr bool operator==(OpaaxStringView Other) const noexcept
        {
            return m_Length == Other.m_Length && Matches(0, Other);
        }

        constexpr bool operator!=(OpaaxStringView Other) const noexcept { return !(*this == Other); }

        // Writes exactly GetLength() bytes.
        friend std::ostream& operator<<(std::ostream& OS, OpaaxStringView View)
        {
            if (View.m_Length > 0) { OS.write(View.m_Data, View.m_Length); }
            return OS;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        const char* m_Data   = nullptr;
        Uint32      m_Length = 0;
    };
} // namespace Opaax

// Formatter for spdlog / fmtlib (no copy, explicit length).
#include <spdlog/fmt/fmt.h>

template <>
struct fmt::formatter<Opaax::OpaaxStringView> : fmt::formatter<fmt::string_view>
{
    auto format(Opaax::OpaaxStringView View, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(View.Data(), View.GetLength()), CTX);
    }
};
