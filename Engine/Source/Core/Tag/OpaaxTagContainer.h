#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Tag/OpaaxTag.h"

namespace Opaax
{
    /**
     * A set of OpaaxTags (like Unreal's FGameplayTagContainer). Stores exactly what was
     * added, in order, without duplicates; parents are matched through OpaaxTag::MatchesTag.
     * @see Core/Tag/OpaaxTagJson.h for JSON.
     */
    class OpaaxTagContainer final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxTagContainer() = default;

        /** OpaaxTagContainer{"Damage.Fire", "Element.Fire"}. Invalid entries are dropped. */
        OpaaxTagContainer(TInitArray<OpaaxTag> InTags)
        {
            m_Tags.reserve(InTags.size());
            for (const OpaaxTag lTag : InTags) { AddTag(lTag); }
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** @return False if InTag is invalid or already present. */
        bool AddTag(OpaaxTag InTag)
        {
            if (!InTag.IsValid() || HasTagExact(InTag)) { return false; }

            m_Tags.emplace_back(InTag);
            return true;
        }

        /** Removes the exact tag only ("Damage" does not remove "Damage.Fire"). */
        bool RemoveTag(OpaaxTag InTag)
        {
            for (Uint32 i = 0; i < Num(); ++i)
            {
                if (m_Tags[i] == InTag)
                {
                    m_Tags.erase(m_Tags.begin() + i);
                    return true;
                }
            }
            return false;
        }

        void Clear() noexcept { m_Tags.clear(); }

        /** Adds every tag InOther holds that this one does not. */
        void Append(const OpaaxTagContainer& InOther)
        {
            for (const OpaaxTag lTag : InOther) { AddTag(lTag); }
        }

        /**
         * True if any tag is InTag or a descendant of it.
         */
        bool HasTag(OpaaxTag InTag) const noexcept
        {
            for (const OpaaxTag lTag : m_Tags)
            {
                if (lTag.MatchesTag(InTag)) { return true; }
            }
            return false;
        }

        bool HasTagExact(OpaaxTag InTag) const noexcept
        {
            for (const OpaaxTag lTag : m_Tags)
            {
                if (lTag == InTag) { return true; }
            }
            return false;
        }

        /** Hierarchical. False if InOther is empty. */
        bool HasAny(const OpaaxTagContainer& InOther) const noexcept
        {
            for (const OpaaxTag lTag : InOther)
            {
                if (HasTag(lTag)) { return true; }
            }
            return false;
        }

        /** Hierarchical. True if InOther is empty. */
        bool HasAll(const OpaaxTagContainer& InOther) const noexcept
        {
            for (const OpaaxTag lTag : InOther)
            {
                if (!HasTag(lTag)) { return false; }
            }
            return true;
        }

        /** "Damage.Fire, Element.Fire" */
        OpaaxString ToString() const
        {
            OpaaxString lResult;
            for (const OpaaxTag lTag : m_Tags)
            {
                if (!lResult.IsEmpty()) { lResult += ", "; }
                lResult += lTag.ToString();
            }
            return lResult;
        }

        // ----------------------------------------------------------------------------
        // Get - Set
    public:
        Uint32 Num()     const noexcept { return static_cast<Uint32>(m_Tags.size()); }
        bool   IsEmpty() const noexcept { return m_Tags.empty(); }

        const OpaaxTag* begin() const noexcept { return m_Tags.data(); }
        const OpaaxTag* end()   const noexcept { return m_Tags.data() + m_Tags.size(); }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        /** Same tags, in any order. */
        bool operator==(const OpaaxTagContainer& Other) const noexcept
        {
            if (Num() != Other.Num()) { return false; }

            for (const OpaaxTag lTag : m_Tags)
            {
                if (!Other.HasTagExact(lTag)) { return false; }
            }
            return true;
        }

        bool operator!=(const OpaaxTagContainer& Other) const noexcept { return !(*this == Other); }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<OpaaxTag> m_Tags;
    };
} // namespace Opaax
