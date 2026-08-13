#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"

namespace Opaax
{
    /** The one byte that separates a tag's segments. */
    inline constexpr char TAG_SEPARATOR = '.';

    /**
     * @class OpaaxTag
     *
     * Hierarchical gameplay label — "Damage.Fire.Burn" — in 4 bytes. Unreal's FGameplayTag, minus
     * the registry. Trivially copyable, pass BY VALUE.
     *
     * THERE IS NO TAG REGISTRY, and that is the design (I14). The hierarchy is DERIVED from the
     * dotted text: a tag IS-A another when its text starts with the other's AND the next byte is a
     * separator. So there is no declaration table, no ini file, no boot ordering, and no new mutable
     * static (I1) — the interned name is the only state, and OpaaxStringID already owns it.
     * The price, stated plainly: a MISSPELLED tag is a perfectly valid tag that simply matches
     * nothing, and there is no editor dropdown to pick from. Structural typos are caught (see
     * IsValidTagText); semantic ones are not.
     *
     * COST — the ctor INTERNS, so a tag that appears in a per-frame test belongs in a member or a
     * file-scope `static const OpaaxTag`, not rebuilt inside the loop. Exact comparison (operator==)
     * is a single integer compare; MatchesTag is one prefix compare over the pool's bytes.
     *
     * Header-only and stateless, so no OPAAX_API (I6) — nothing to unify across the DLL line.
     *
     * @see OpaaxTagContainer for the set form (HasTag / HasAny / HasAll).
     * @see Core/Tag/OpaaxTagJson.h for the nlohmann bridge (kept apart so matching costs no json).
     */
    class OpaaxTag final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /** The invalid tag: it matches nothing, and nothing matches it. */
        constexpr OpaaxTag() noexcept = default;

        /**
         * Implicit on purpose, so `HasTag("Damage.Fire")` reads the way it should.
         *
         * Empty text is the invalid tag, quietly. MALFORMED text is the invalid tag AND asserts,
         * which is the right trade for a literal written by hand. Anything coming from a FILE or a
         * TEXT FIELD is untrusted: gate it on IsValidTagText first, the way from_json does.
         */
        OpaaxTag(OpaaxStringView InText) : m_Name(Intern(InText)) {}

        // A literal and an OpaaxString each get their OWN ctor rather than riding the view above:
        // two user-defined conversions never chain, so `const char*` -> view -> tag would not
        // compile at a call site. Same reason OpaaxStringID carries the same pair.
        OpaaxTag(const char*        InText) : OpaaxTag(OpaaxStringView(InText)) {}
        OpaaxTag(const OpaaxString& InText) : OpaaxTag(OpaaxStringView(InText)) {}

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        static OpaaxStringID Intern(OpaaxStringView InText)
        {
            if (InText.IsEmpty()) { return OpaaxStringID(); }

            // Loud in dev, because with no registry this assert IS the typo net. Core cannot log
            // (I11), so an assert is the only way to be loud from here.
            if (!IsValidTagText(InText))
            {
                OPAAX_ASSERT(false);
                return OpaaxStringID();
            }

            return OpaaxStringID(OpaaxString(InText));
        }

    public:
        /**
         * Structural validity, not spelling: non-empty, no leading or trailing separator, no empty
         * segment ("A..B"), no whitespace or control bytes.
         *
         * These rules are load-bearing rather than tidy — an empty segment would produce a parent
         * that is not a prefix of its own child, and a stray space interns as a distinct name that
         * nothing will ever match.
         *
         * UTF-8 SAFE, and only because the scan is unsigned (I7): '.' and the control bytes are
         * ASCII, which never appear inside a multi-byte sequence — but `char` is signed on MSVC, so
         * a plain `lChar <= ' '` would read every continuation byte as negative and refuse any
         * non-ASCII tag name. Same reason PathString::Stem may scan for separators byte-wise.
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
         * Hierarchical match: is this tag InParent, or a DESCENDANT of it?
         * "Damage.Fire.Burn" matches "Damage.Fire" and "Damage"; "DamageOverTime" matches neither.
         *
         * An EXACT hit answers before touching the pool; only the descendant test reads the text,
         * which costs the pool's shared lock. Prefer operator== where equality is what you mean.
         *
         * @return false whenever either side is invalid — an absent tag is inert in both directions.
         */
        bool MatchesTag(OpaaxTag InParent) const noexcept
        {
            if (!IsValid() || !InParent.IsValid()) { return false; }
            if (m_Name == InParent.m_Name) { return true; }

            const OpaaxStringView lSelf   = GetView();
            const OpaaxStringView lParent = InParent.GetView();

            // The separator check is the whole rule: a prefix only names an ancestor when it ends on
            // a segment boundary.
            return lSelf.GetLength() > lParent.GetLength()
                && lSelf[lParent.GetLength()] == TAG_SEPARATOR
                && lSelf.StartsWith(lParent);
        }

        /**
         * "Damage.Fire.Burn" -> "Damage.Fire". A root tag has no parent.
         *
         * Interns the parent if nobody named it before — a parent is a real name, so it earns a real
         * slot. Repeat calls are lookups.
         *
         * @return The invalid tag at the root.
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
         * The full dotted text, borrowed.
         *
         * Safe to hold, unlike most views (see OpaaxStringView's lifetime note): it points into the
         * intern pool, whose entries are immortal and never move.
         *
         * @return An EMPTY view for the invalid tag — deliberately not the pool's "None" placeholder,
         *   which would otherwise make the invalid tag read as an ancestor of anything called None.
         */
        OpaaxStringView GetView() const noexcept
        {
            return IsValid() ? m_Name.GetView() : OpaaxStringView();
        }

        /** A displayable copy. The invalid tag prints as "None", like every other unset id (I11). */
        OpaaxString ToString() const { return m_Name.ToString(); }

        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        /** The interned name — the tag's identity, and what every comparison here compares. */
        constexpr OpaaxStringID GetName() const noexcept { return m_Name; }

        constexpr bool IsValid() const noexcept { return m_Name.IsValid(); }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        /** EXACT match, one integer compare. MatchesTag is the hierarchical form. */
        constexpr bool operator==(OpaaxTag Other) const noexcept { return m_Name == Other.m_Name; }
        constexpr bool operator!=(OpaaxTag Other) const noexcept { return m_Name != Other.m_Name; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID m_Name;
    };
} // namespace Opaax

// std::hash — the interned id is already a well-distributed table index, so it IS the hash.
template <>
struct std::hash<Opaax::OpaaxTag>
{
    size_t operator()(Opaax::OpaaxTag Tag) const noexcept
    {
        return static_cast<size_t>(Tag.GetName().GetId());
    }
};

// Formatter for spdlog / fmtlib — a view over the pool's bytes, never a copy. Goes through the
// interned name rather than GetView(), so an invalid tag logs as "None" instead of vanishing.
#include <spdlog/fmt/fmt.h>

template <>
struct fmt::formatter<Opaax::OpaaxTag> : fmt::formatter<fmt::string_view>
{
    auto format(Opaax::OpaaxTag Tag, format_context& CTX) const
    {
        return fmt::formatter<fmt::string_view>::format(fmt::string_view(Tag.GetName().CStr()), CTX);
    }
};
