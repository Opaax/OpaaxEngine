#pragma once

#include <ostream>      // operator<<
#include <string>       // std::char_traits — constexpr length/compare, and the std::string ctor
#include <string_view>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class OpaaxString;

    /**
     * @class OpaaxStringView
     *
     * Non-owning view over bytes somebody else owns: {const char*, Uint32}, 16 bytes, trivially
     * copyable, constexpr throughout. Pass it BY VALUE.
     *
     * It speaks the engine's vocabulary — Uint32 lengths, Int32/-1 search results, the same member
     * names OpaaxString uses — so std::string_view appears only where a vendor demands it (fmt,
     * nlohmann, entt). That is what the implicit conversions are for, in both directions.
     *
     * NOT NULL-TERMINATED, which is why there is deliberately no CStr(). A view is usually a SLICE
     * of a longer buffer, so the byte past the end belongs to somebody else: no member here calls
     * strlen/strcmp/strstr, and anything that needs a terminator goes through ToString(). Handing
     * Data() to a C API is the one way to misuse this type.
     *
     * LIFETIME — it owns nothing and cannot know when the bytes die. Same contract as
     * std::string_view: fine as a parameter, dangerous as a member or a return value that outlives
     * its source. `OpaaxStringView lView = MakeString();` dangles.
     *
     * Header-only and stateless, so no OPAAX_API (I6) — there is no state to unify across the DLL
     * line and nothing for a consumer to duplicate.
     *
     * @see OpaaxString for the owning form; ToString() converts.
     * @see Core/Hash/OpaaxHash.h for std::hash<OpaaxStringView> (it lives there to avoid a cycle).
     */
    class OpaaxStringView final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        constexpr OpaaxStringView() noexcept = default;

        /** Null-terminated bytes. nullptr is an empty view, not a crash. */
        constexpr OpaaxStringView(const char* InStr) noexcept
            : m_Data(InStr)
            , m_Length(InStr ? static_cast<Uint32>(std::char_traits<char>::length(InStr)) : 0) {}

        /** Counted — InStr need not be null-terminated, and nothing past InLength is ever read. */
        constexpr OpaaxStringView(const char* InStr, Uint32 InLength) noexcept
            : m_Data(InStr), m_Length(InStr ? InLength : 0) {}

        // Implicit on purpose: these are the vendor boundary (entt::type_name, nlohmann, fmt).
        constexpr OpaaxStringView(std::string_view InView) noexcept
            : m_Data(InView.data()), m_Length(static_cast<Uint32>(InView.size())) {}

        OpaaxStringView(const std::string& InStr) noexcept
            : m_Data(InStr.data()), m_Length(static_cast<Uint32>(InStr.size())) {}

        // An OpaaxString converts through its own operator OpaaxStringView() — see OpaaxString.hpp.

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // Every comparison funnels here so exactly one place decides that a zero-length compare
        // never dereferences (char_traits::compare with a null pointer is UB even for count 0).
        constexpr bool Matches(Uint32 InAt, OpaaxStringView InOther) const noexcept
        {
            return InOther.m_Length == 0
                       || std::char_traits<char>::compare(m_Data + InAt, InOther.m_Data, InOther.m_Length) == 0;
        }

    public:
        /** A COPY of the viewed bytes, null-terminated. Defined in OpaaxString.hpp (I11). */
        OpaaxString ToString() const;

        /**
         * A view of the same bytes — free, no allocation, unlike OpaaxString::SubString.
         * @return An empty view when InStart is past the end.
         */
        constexpr OpaaxStringView SubString(Uint32 InStart, Uint32 InLength = UINT32_MAX) const noexcept
        {
            if (InStart >= m_Length) { return OpaaxStringView(); }

            // Clamp against the REMAINDER, never against InStart + InLength: that sum wraps for a
            // large InLength and hands back a view past the end (same rule as OpaaxString).
            const Uint32 lRemaining = m_Length - InStart;
            const Uint32 lActual    = (InLength > lRemaining) ? lRemaining : InLength;

            return OpaaxStringView(m_Data + InStart, lActual);
        }

        /** Narrow this view in place. Over-long counts clamp — consuming more than there is empties it. */
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
         * @return Index of the first match at or after InStartPos, or -1. An empty needle answers
         *   InStartPos, matching std::string_view::find.
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

        /** @return Index of the LAST match, or -1. An empty needle answers the length. */
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

        /** @return Index of the last byte that is ANY of InChars, or -1. Path separators, mainly. */
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

        /** The viewed bytes. NOT null-terminated — never hand this to a C string API. */
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

        /** The vendor boundary, in one place. */
        constexpr operator std::string_view() const noexcept
        {
            return m_Data ? std::string_view(m_Data, m_Length) : std::string_view();
        }

        // One overload is enough: a literal, a const char*, an OpaaxString and a std::string_view
        // all convert to a view already.
        constexpr bool operator==(OpaaxStringView Other) const noexcept
        {
            return m_Length == Other.m_Length && Matches(0, Other);
        }

        constexpr bool operator!=(OpaaxStringView Other) const noexcept { return !(*this == Other); }

        // Writes exactly GetLength() bytes — a view is not terminated, so `OS << Data()` would run on.
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

// Formatter for spdlog / fmtlib — a view over bytes we already hold, never a copy.
// Length is passed explicitly: the bytes are not terminated, so fmt must not go looking for one.
// See the matching note in OpaaxString.hpp for why the copy is worth avoiding.
#include <spdlog/fmt/fmt.h>

template <>
struct fmt::formatter<Opaax::OpaaxStringView> : fmt::formatter<fmt::string_view>
{
    auto format(Opaax::OpaaxStringView View, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(View.Data(), View.GetLength()), CTX);
    }
};
