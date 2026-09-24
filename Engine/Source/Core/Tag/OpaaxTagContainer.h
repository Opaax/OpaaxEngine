#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Tag/OpaaxTag.h"

namespace Opaax
{
    /**
     * @class OpaaxTagContainer
     *
     * A set of OpaaxTags — "what is this thing?" answered as a list. Unreal's FGameplayTagContainer.
     *
     * STORES EXACTLY WHAT WAS ADDED. Unreal expands and keeps every parent so HasTag is a hash hit;
     * here the parents are implied by OpaaxTag::MatchesTag and a linear scan answers it, which at the
     * handful-of-tags-per-entity scale this engine works at is both faster and honest — what you read
     * back is what you put in, and Add/Remove need no bookkeeping to stay consistent.
     *
     * Duplicates are refused, so the order is registration order and nothing else.
     *
     * Header-only and stateless, so no OPAAX_API (I6).
     *
     * @see Core/Tag/OpaaxTagJson.h for the nlohmann bridge.
     */
    class OpaaxTagContainer final
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpaaxTagContainer() = default;

        /** Braced init — `OpaaxTagContainer{"Damage.Fire", "Element.Fire"}`. Invalid entries drop. */
        OpaaxTagContainer(TInitArray<OpaaxTag> InTags)
        {
            m_Tags.reserve(InTags.size());
            for (const OpaaxTag lTag : InTags) { AddTag(lTag); }
        }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** @return false when InTag is invalid or already held. */
        bool AddTag(OpaaxTag InTag)
        {
            if (!InTag.IsValid() || HasTagExact(InTag)) { return false; }

            m_Tags.emplace_back(InTag);
            return true;
        }

        /** Removes the EXACT tag — removing "Damage" never touches "Damage.Fire". */
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
         * Hierarchical: true when ANY held tag is InTag or a descendant of it, so a container
         * holding "Damage.Fire.Burn" answers true for "Damage".
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

        /** Hierarchical. An EMPTY InOther asks for nothing, so nothing satisfies it: false. */
        bool HasAny(const OpaaxTagContainer& InOther) const noexcept
        {
            for (const OpaaxTag lTag : InOther)
            {
                if (HasTag(lTag)) { return true; }
            }
            return false;
        }

        /** Hierarchical. An EMPTY InOther demands nothing, so it is trivially satisfied: true. */
        bool HasAll(const OpaaxTagContainer& InOther) const noexcept
        {
            for (const OpaaxTag lTag : InOther)
            {
                if (!HasTag(lTag)) { return false; }
            }
            return true;
        }

        /** "Damage.Fire, Element.Fire" — for logs and the Inspector. */
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
        /** Order-insensitive and EXACT: same tags, however they were added. */
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
