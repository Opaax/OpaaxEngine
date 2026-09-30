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

        // A failed load is kept too (empty), so it is not retried for every placement.
        TUniquePtr<Claim> lClaim = MakeUnique<Claim>();
        lClaim->Path = InAssetPath;
        lClaim->Ref  = m_Resources.Load<PrefabResource>(m_Paths.AssetToAbsolute(InAssetPath).CStr());

        // Kept by pointer: flattening recurses into Resolve, which grows the list.
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
            // A cycle (A places B places A): error, and this placement expands to nothing.
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

        // Depth-first over the raw records, with a visited list so a cycle ends.
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
