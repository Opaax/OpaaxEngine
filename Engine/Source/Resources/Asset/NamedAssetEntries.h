#pragma once

#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxPathString.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/Object.h"

namespace Opaax
{
    // =============================================================================
    // NamedAssetEntries — a list whose entries each give another asset a short name, like a Mover's
    //   modes or a library's clips: gameplay asks for OPAAX_ID("Run"), not a path. Names resolve by
    //   first match. TEntry has an OpaaxStringID Name and an asset reference (a TAssetRef).
    // =============================================================================
    namespace NamedAssetEntries
    {
        /** The entry named InName, or nullptr. Exact match only. */
        template<typename TEntry>
        const TEntry* FindExact(const TDynArray<TEntry>& InEntries, const OpaaxStringID InName) noexcept
        {
            if (!InName.IsValid())
            {
                return nullptr;
            }

            for (const TEntry& lEntry : InEntries)
            {
                if (lEntry.Name == InName)
                {
                    return &lEntry;
                }
            }

            return nullptr;
        }

        /**
         * The entry for InName. An invalid id gives InDefault's entry, then the first entry. An unknown
         * name gives nullptr (no silent fallback).
         */
        template<typename TEntry>
        const TEntry* Find(const TDynArray<TEntry>& InEntries, const OpaaxStringID InName,
                           const OpaaxStringID InDefault) noexcept
        {
            if (InName.IsValid())
            {
                return FindExact(InEntries, InName);
            }

            if (const TEntry* lDefault = FindExact(InEntries, InDefault))
            {
                return lDefault;
            }

            return InEntries.empty() ? nullptr : &InEntries[0];
        }

        /**
         * Keeps the names usable after an edit: a later entry repeating a name loses it, an entry with an
         * asset and no name takes its file's name when free, and a default naming no entry is cleared (the
         * first entry plays). Each fix is logged under InLog, naming InOwner.
         * @param InAssetOf The entry's asset reference (&MoverEntry::ModeAsset)
         */
        template<typename TEntry, typename TRef>
        void FixUpAfterEdit(TDynArray<TEntry>& InOutEntries, OpaaxStringID& InOutDefault, TRef TEntry::* InAssetOf,
                            const Object& InOwner, const LogCategory& InLog)
        {
            // Names resolve by first match: a later entry with the same name could never be reached.
            for (Uint64 lIndex = 1; lIndex < InOutEntries.size(); ++lIndex)
            {
                for (Uint64 lEarlier = 0; lEarlier < lIndex; ++lEarlier)
                {
                    if (InOutEntries[lIndex].Name.IsValid() && InOutEntries[lIndex].Name == InOutEntries[lEarlier].Name)
                    {
                        OPAAX_LOG(InLog, Warn, "{} '{}': entry {} repeats the name '{}' — its name is cleared",
                                  InOwner.GetClass().GetName(), InOwner.GetName(), lIndex, InOutEntries[lIndex].Name);
                        InOutEntries[lIndex].Name = OpaaxStringID();
                        break;
                    }
                }
            }

            // An asset dropped on an unnamed entry names it, as its file does.
            for (TEntry& lEntry : InOutEntries)
            {
                const TRef& lAsset = lEntry.*InAssetOf;
                if (lEntry.Name.IsValid() || lAsset.IsEmpty())
                {
                    continue;
                }

                const OpaaxStringID lStem(PathString::Stem(lAsset.Path).ToString());
                if (lStem.IsValid() && FindExact(InOutEntries, lStem) == nullptr)
                {
                    lEntry.Name = lStem;
                }
            }

            // A default naming no entry would silently play the first one.
            if (InOutDefault.IsValid() && FindExact(InOutEntries, InOutDefault) == nullptr)
            {
                OPAAX_LOG(InLog, Warn, "{} '{}': no entry is named '{}' — the default is cleared (the first entry)",
                          InOwner.GetClass().GetName(), InOwner.GetName(), InOutDefault);
                InOutDefault = OpaaxStringID();
            }
        }
    }
}
