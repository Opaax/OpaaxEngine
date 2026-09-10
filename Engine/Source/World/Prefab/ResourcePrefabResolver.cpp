#include "World/Prefab/ResourcePrefabResolver.h"

#include "Application/Services/IPaths.h"
#include "World/Prefab/PrefabFactory.h"

namespace Opaax
{
    ResourcePrefabResolver::Claim& ResourcePrefabResolver::ClaimFor(const OpaaxString& InAssetPath) const
    {
        for (const TUniquePtr<Claim>& lClaim : m_Claims)
        {
            if (lClaim->Path == InAssetPath) { return *lClaim; }
        }

        // A FAILED load is claimed too, as an empty ref — otherwise a map with forty placements
        // of a deleted prefab would retry the file forty times and log forty errors.
        TUniquePtr<Claim> lClaim = MakeUnique<Claim>();
        lClaim->Path = InAssetPath;
        lClaim->Ref  = m_Resources.Load<PrefabResource>(m_Paths.AssetToAbsolute(InAssetPath).CStr());

        // Held by POINTER across the flatten: it recurses into Resolve for nested paths, which
        // grow the list, and the claim itself stays put on the heap.
        Claim* const lNew = lClaim.get();
        m_Claims.emplace_back(Move(lClaim));

        if (const PrefabResource* const lRaw = lNew->Ref.Get())
        {
            lNew->bInFlight = true;
            lNew->Flattened = MakeUnique<PrefabData>(
                PrefabFactory::Flatten(lRaw->Data, InAssetPath, *this, m_Components));
            lNew->bInFlight = false;
        }

        return *lNew;
    }

    const PrefabData* ResourcePrefabResolver::Resolve(const OpaaxString& InAssetPath) const
    {
        if (InAssetPath.IsEmpty()) { return nullptr; }

        const Claim& lClaim = ClaimFor(InAssetPath);

        if (lClaim.bInFlight)
        {
            // Reached from inside its own flatten: A places B places A. Loud, and the placement
            // that closes the loop expands to nothing — the rest of both prefabs survives.
            OPAAX_LOG(LogPrefabFold, Error,
                      "Prefab '{}' places itself, directly or through another prefab — that placement "
                      "is refused", InAssetPath.CStr());
            return nullptr;
        }

        return lClaim.Flattened.get();
    }

    bool ResourcePrefabResolver::Places(const OpaaxString& InOuter, const OpaaxString& InInner) const
    {
        if (InOuter.IsEmpty() || InInner.IsEmpty()) { return false; }

        // Over the RAW records, depth-first, with a visited list so a cycle terminates.
        TDynArray<OpaaxString> lVisited;
        TDynArray<OpaaxString> lPending;
        lPending.emplace_back(InOuter);

        while (!lPending.empty())
        {
            const OpaaxString lPath = Move(lPending.back());
            lPending.pop_back();

            bool lSeen = false;
            for (const OpaaxString& lDone : lVisited) { if (lDone == lPath) { lSeen = true; break; } }
            if (lSeen) { continue; }
            lVisited.emplace_back(lPath);

            const PrefabResource* const lRaw = ClaimFor(lPath).Ref.Get();
            if (lRaw == nullptr) { continue; }

            for (const PrefabInstanceRecord& lRecord : lRaw->Data.Instances)
            {
                if (lRecord.Prefab == InInner) { return true; }
                lPending.emplace_back(lRecord.Prefab);
            }
        }

        return false;
    }
}
