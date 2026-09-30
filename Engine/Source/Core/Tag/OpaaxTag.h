#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    /** Separator between tag segments. */
    inline constexpr char TAG_SEPARATOR = '.';

    /**
     * Hierarchical gameplay tag ("Damage.Fire.Burn"), 4 bytes. Like Unreal's FGameplayTag,
     * without a registry: the hierarchy comes from the dotted text. A misspelled tag is valid
     * but matches nothing.
     *
     * The constructor interns the text, so keep tags used every frame in a member or a static.
     *
     * @see OpaaxTagContainer for a set of tags.
     * @see Core/Tag/OpaaxTagJson.h for JSON.
     */
    class OpaaxTag final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /** Invalid tag: matches nothing. */
        constexpr OpaaxTag() noexcept = default;

        /**
         * Implicit, so HasTag("Damage.Fire") works. Empty text gives the invalid tag;
         * malformed text also asserts. Check untrusted text with IsValidTagText first.
         */
        OpaaxTag(OpaaxStringView InText) : m_Name(Intern(InText)) {}

        // Separate const char* and OpaaxString constructors: conversions do not chain.
        OpaaxTag(const char*        InText) : OpaaxTag(OpaaxStringView(InText)) {}
        OpaaxTag(const OpaaxString& InText) : OpaaxTag(OpaaxStringView(InText)) {}

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        static OpaaxStringID Intern(OpaaxStringView InText)
        {
            if (InText.IsEmpty()) { return OpaaxStringID(); }

            // Assert on malformed text: the only typo check there is.
            if (!IsValidTagText(InText))
            {
                OPAAX_ASSERT(false);
                return OpaaxStringID();
            }

            return OpaaxStringID(OpaaxString(InText));
        }

    public:
        /**
         * Structural check: non-empty, no leading/trailing separator, no empty segment ("A..B"),
         * no whitespace or control characters. UTF-8 safe.
         */
        static constexpr bool IsValidTagText(OpaaxStringView InText) noexcept
        {
            if (InText.IsEmpty()) { return false; }
            if (InText[0] == TAG_SEPARATOR) { return false; }
            if (InText[InText.GetLength() - 1] == TAG_SEPARATOR) { return false; }

            for (Uint32 i = 0; i < InText.GetLength(); ++i)
            {
                const char lChar = InText[i];

                if (static_cast<unsigned char>(lChar) <= ' ') { return false; }

                // i is never 0 here: a leading separator was refused above.
                if (lChar == TAG_SEPARATOR && InText[i - 1] == TAG_SEPARATOR) { return false; }
            }

            return true;
        }

        /**
         * True if this tag is InParent or a descendant of it.
         * "Damage.Fire.Burn" matches "Damage.Fire" and "Damage"; "DamageOverTime" matches neither.
         * @return False if either tag is invalid
         */
        bool MatchesTag(OpaaxTag InParent) const noexcept
        {
            if (!IsValid() || !InParent.IsValid()) { return false; }
            if (m_Name == InParent.m_Name) { return true; }

            const OpaaxStringView lSelf   = GetView();
            const OpaaxStringView lParent = InParent.GetView();

            // A prefix is an ancestor only if it ends on a separator.
            return lSelf.GetLength() > lParent.GetLength()
                && lSelf[lParent.GetLength()] == TAG_SEPARATOR
                && lSelf.StartsWith(lParent);
        }

        /**
         * "Damage.Fire.Burn" -> "Damage.Fire". Interns the parent.
         * @return The invalid tag for a root tag
         */
        OpaaxTag GetParent() const
        {
            const OpaaxStringView lSelf = GetView();
            const Int32           lDot  = lSelf.FindLast(TAG_SEPARATOR);

            return (lDot <= 0) ? OpaaxTag() : OpaaxTag(lSelf.SubString(0, static_cast<Uint32>(lDot)));
        }

        /** "Damage.Fire.Burn" -> "Burn". A root tag is its own leaf. */
        OpaaxStringView GetLeafName() const noexcept
        {
            const OpaaxStringView lSelf = GetView();
            const Int32           lDot  = lSelf.FindLast(TAG_SEPARATOR);

            return (lDot < 0) ? lSelf : lSelf.SubString(static_cast<Uint32>(lDot) + 1u);
        }

        /**
         * The full text. Safe to keep (points into the intern pool).
         * @return Empty for the invalid tag
         */
        OpaaxStringView GetView() const noexcept
        {
            return IsValid() ? m_Name.GetView() : OpaaxStringView();
        }

        /** A copy of the text. The invalid tag gives "None". */
        OpaaxString ToString() const { return m_Name.ToString(); }

        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        /** The interned name. */
        constexpr OpaaxStringID GetName() const noexcept { return m_Name; }

        constexpr bool IsValid() const noexcept { return m_Name.IsValid(); }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        /** Exact match. MatchesTag is the hierarchical version. */
        constexpr bool operator==(OpaaxTag Other) const noexcept { return m_Name == Other.m_Name; }
        constexpr bool operator!=(OpaaxTag Other) const noexcept { return m_Name != Other.m_Name; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID m_Name;
    };
} // namespace Opaax

// std::hash<OpaaxTag>: the interned id is already a good hash.
template <>
struct std::hash<Opaax::OpaaxTag>
{
    size_t operator()(Opaax::OpaaxTag Tag) const noexcept
    {
        return static_cast<size_t>(Tag.GetName().GetId());
    }
};

// Formatter for spdlog / fmtlib. An invalid tag logs as "None".
#include <spdlog/fmt/fmt.h>

template <>
struct fmt::formatter<Opaax::OpaaxTag> : fmt::formatter<fmt::string_view>
{
    auto format(Opaax::OpaaxTag Tag, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Tag.GetName().CStr()), CTX);
    }
};
