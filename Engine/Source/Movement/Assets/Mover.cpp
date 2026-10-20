#include "Movement/Assets/Mover.h"

#include "Resources/Asset/NamedAssetEntries.h"

namespace Opaax
{
    const MoverEntry* Mover::FindExact(const OpaaxStringID InName) const noexcept
    {
        return NamedAssetEntries::FindExact(Entries, InName);
    }

    const MoverEntry* Mover::Find(const OpaaxStringID InName) const noexcept
    {
        return NamedAssetEntries::Find(Entries, InName, DefaultMode);
    }

    void Mover::PostEditChange()
    {
        NamedAssetEntries::FixUpAfterEdit(Entries, DefaultMode, &MoverEntry::ModeAsset, *this, LogMoverAsset);
    }
}
