#include "Movement/Assets/Mover.h"

#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
    const MoverEntry* Mover::FindExact(const OpaaxStringID InName) const noexcept
    {
        if (!InName.IsValid())
        {
            return nullptr;
        }

        for (const MoverEntry& lEntry : Entries)
        {
            if (lEntry.Name == InName)
            {
                return &lEntry;
            }
        }

        return nullptr;
    }

    const MoverEntry* Mover::Find(const OpaaxStringID InName) const noexcept
    {
        if (InName.IsValid())
        {
            return FindExact(InName);
        }

        if (const MoverEntry* lDefault = FindExact(DefaultMode))
        {
            return lDefault;
        }

        return Entries.empty() ? nullptr : &Entries[0];
    }

    void Mover::PostEditChange()
    {
        // Names resolve by first match: a later entry with the same name could never be reached.
        for (Uint64 lIndex = 1; lIndex < Entries.size(); ++lIndex)
        {
            for (Uint64 lEarlier = 0; lEarlier < lIndex; ++lEarlier)
            {
                if (Entries[lIndex].Name.IsValid() && Entries[lIndex].Name == Entries[lEarlier].Name)
                {
                    OPAAX_LOG(LogMoverAsset, Warn, "Mover '{}': entry {} repeats the name '{}' — its name is cleared",
                              GetName(), lIndex, Entries[lIndex].Name);
                    Entries[lIndex].Name = OpaaxStringID();
                    break;
                }
            }
        }

        // A tuning dropped on an unnamed entry names it, as its file does.
        for (MoverEntry& lEntry : Entries)
        {
            if (lEntry.Name.IsValid() || lEntry.ModeAsset.IsEmpty())
            {
                continue;
            }

            const OpaaxStringID lStem(PathString::Stem(lEntry.ModeAsset.Path).ToString());
            if (lStem.IsValid() && FindExact(lStem) == nullptr)
            {
                lEntry.Name = lStem;
            }
        }

        // A default naming no entry would silently start in the first one.
        if (DefaultMode.IsValid() && FindExact(DefaultMode) == nullptr)
        {
            OPAAX_LOG(LogMoverAsset, Warn, "Mover '{}': no entry is named '{}' — the default is cleared (the first entry)",
                      GetName(), DefaultMode);
            DefaultMode = OpaaxStringID();
        }
    }
}
