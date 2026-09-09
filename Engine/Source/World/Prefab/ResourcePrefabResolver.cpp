#include "World/Prefab/ResourcePrefabResolver.h"

#include "Application/Services/IPaths.h"

namespace Opaax
{
    const PrefabData* ResourcePrefabResolver::Resolve(const OpaaxString& InAssetPath) const
    {
        if (InAssetPath.IsEmpty()) { return nullptr; }

        for (const Claim& lClaim : m_Claims)
        {
            if (lClaim.Path != InAssetPath) { continue; }

            // A FAILED load is cached too, as an empty ref — otherwise a map with forty placements
            // of a deleted prefab would retry the file forty times and log forty errors.
            const PrefabResource* const lCached = lClaim.Ref.IsValid() ? lClaim.Ref.Get() : nullptr;
            return (lCached != nullptr) ? &lCached->Data : nullptr;
        }

        const OpaaxString lAbsPath = m_Paths.AssetToAbsolute(InAssetPath);

        Claim lClaim;
        lClaim.Path = InAssetPath;
        lClaim.Ref  = m_Resources.Load<PrefabResource>(lAbsPath.CStr());

        m_Claims.emplace_back(Move(lClaim));

        const PrefabResource* const lLoaded = m_Claims.back().Ref.Get();

        return (lLoaded != nullptr) ? &lLoaded->Data : nullptr;
    }
}
