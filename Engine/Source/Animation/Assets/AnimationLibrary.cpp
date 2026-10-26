#include "Animation/Assets/AnimationLibrary.h"

#include "Resources/Asset/NamedAssetEntries.h"

namespace Opaax
{
    const AnimationLibraryEntry* AnimationLibrary::FindExact(const OpaaxStringID InName) const noexcept
    {
        return NamedAssetEntries::FindExact(Entries, InName);
    }

    const AnimationLibraryEntry* AnimationLibrary::Find(const OpaaxStringID InName) const noexcept
    {
        return NamedAssetEntries::Find(Entries, InName, DefaultClip);
    }

    void AnimationLibrary::PostEditChange()
    {
        NamedAssetEntries::FixUpAfterEdit(Entries, DefaultClip, &AnimationLibraryEntry::Clip, *this, LogAnimationLibrary);
    }
}
