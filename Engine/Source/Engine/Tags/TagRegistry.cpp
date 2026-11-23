#include "Engine/Tags/TagRegistry.h"

#include "Core/Algo/Algo.h"

namespace Opaax
{
    namespace
    {
        const TagRegistry::Entry* FindIn(const TDynArray<TagRegistry::Entry>& InEntries, const OpaaxTag InTag) noexcept
        {
            for (const TagRegistry::Entry& lEntry : InEntries)
            {
                if (lEntry.Tag == InTag)
                {
                    return &lEntry;
                }
            }
            return nullptr;
        }
    }

    void TagRegistry::AddNative(const OpaaxTag InTag, const OpaaxString& InComment)
    {
        if (m_bSealed || !InTag.IsValid() || FindIn(m_Native, InTag) != nullptr)
        {
            return;
        }

        m_Native.push_back(Entry{ InTag, InComment, ETagSource::Native });
        Rebuild();
    }

    void TagRegistry::SetConfigTags(const TDynArray<TagDefinition>& InTags)
    {
        m_Config.clear();
        for (const TagDefinition& lDefinition : InTags)
        {
            if (lDefinition.Tag.IsValid() && FindIn(m_Config, lDefinition.Tag) == nullptr)
            {
                m_Config.push_back(Entry{ lDefinition.Tag, lDefinition.Comment, ETagSource::Config });
            }
        }
        Rebuild();
    }

    const TagRegistry::Entry* TagRegistry::Find(const OpaaxTag InTag) const noexcept
    {
        return InTag.IsValid() ? FindIn(m_All, InTag) : nullptr;
    }

    TDynArray<TagRegistry::Entry> TagRegistry::Under(const OpaaxTag InRoot) const
    {
        TDynArray<Entry> lUnder;
        for (const Entry& lEntry : m_All)
        {
            if (!InRoot.IsValid() || (lEntry.Tag != InRoot && lEntry.Tag.MatchesTag(InRoot)))
            {
                lUnder.push_back(lEntry);
            }
        }
        return lUnder;
    }

    void TagRegistry::Rebuild()
    {
        // Code's tags first: a tag both declared and listed stays native, with its code comment
        // unless that one is empty.
        TDynArray<Entry> lAll = m_Native;
        for (const Entry& lListed : m_Config)
        {
            const auto lNative = Algo::FindIf(lAll, [&lListed](const Entry& InEntry) { return InEntry.Tag == lListed.Tag; });
            if (lNative == lAll.end())
            {
                lAll.push_back(lListed);
            }
            else if (lNative->Comment.IsEmpty())
            {
                lNative->Comment = lListed.Comment;
            }
        }

        // Every parent exists with its children.
        const Uint64 lExplicit = lAll.size();
        for (Uint64 i = 0; i < lExplicit; ++i)
        {
            for (OpaaxTag lParent = lAll[i].Tag.GetParent(); lParent.IsValid(); lParent = lParent.GetParent())
            {
                if (FindIn(lAll, lParent) == nullptr)
                {
                    lAll.push_back(Entry{ lParent, OpaaxString(), ETagSource::Implied });
                }
            }
        }

        Algo::Sort(lAll, [](const Entry& InA, const Entry& InB) { return InA.Tag.ToString() < InB.Tag.ToString(); });

        m_All = Move(lAll);
        ++m_Revision;
    }
}
