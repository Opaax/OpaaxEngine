#pragma once

#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"

namespace Opaax
{
    // =============================================================================
    // Asset ID canonicalization policy (pure)
    // =============================================================================

    /**
     * Decide an asset's canonical registry ID from the manifest-lookup outcomes.
     *
     * Precedence:
     *   1. a hit by the original ID            -> keep that ID,
     *   2. else a hit by the project-rel path  -> use the matched entry's ID,
     *   3. else                                -> the project-relative path itself is the ID.
     *
     * The manifest LOOKUPS (which are global, AssetManifest-backed) are the caller's job;
     * this function only decides which result wins, so it stays pure + unit-testable.
     * AssetRegistry::Normalize delegates the canonical-ID decision here.
     */
    //------------------------------------------------------------------------------
    inline OpaaxStringID ResolveCanonicalAssetId(OpaaxStringID        InInputId,
                                                 bool                 InMatchedById,
                                                 const OpaaxStringID* InMatchedByPathId,
                                                 const OpaaxString&   InRelPath)
    {
        if (InMatchedById)                  { return InInputId; }
        if (InMatchedByPathId != nullptr)   { return *InMatchedByPathId; }
        return OpaaxStringID(InRelPath);
    }
}
