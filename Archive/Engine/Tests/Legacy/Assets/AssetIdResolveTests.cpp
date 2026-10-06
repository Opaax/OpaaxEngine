// Suite: asset-ID canonicalization precedence policy (Assets/AssetIdResolve.h).
//
// ResolveCanonicalAssetId was extracted out of AssetRegistry::Normalize so the
// precedence rule (id-hit > path-hit > relative-path fallback) is testable WITHOUT the
// global AssetManifest / OpaaxPath state. The test injects the lookup outcomes directly.
// The manifest LOOKUPS themselves (and absolute-path resolution) remain integration-only.
#include <doctest.h>

#include "Assets/AssetIdResolve.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxString.hpp"

using namespace Opaax;

TEST_CASE("ResolveCanonicalAssetId: a manifest ID hit keeps the input ID")
{
    const OpaaxStringID lInput = OPAAX_ID("Textures/Player");
    const OpaaxStringID lId    = ResolveCanonicalAssetId(
        lInput, /*matchedById=*/true, nullptr, OpaaxString("Game/Assets/anything.png"));
    CHECK(lId == lInput);
}

TEST_CASE("ResolveCanonicalAssetId: a path hit (no ID hit) uses the matched entry's ID")
{
    const OpaaxStringID lInput  = OPAAX_ID("Game/Assets/Textures/Enemy.png");
    const OpaaxStringID lPathId = OPAAX_ID("Textures/Enemy");
    const OpaaxStringID lId     = ResolveCanonicalAssetId(
        lInput, /*matchedById=*/false, &lPathId, OpaaxString("Game/Assets/Textures/Enemy.png"));
    CHECK(lId == lPathId);
}

TEST_CASE("ResolveCanonicalAssetId: no hit falls back to the relative path as the ID")
{
    const OpaaxStringID lInput = OPAAX_ID("Game/Assets/Unlisted.png");
    const OpaaxStringID lId    = ResolveCanonicalAssetId(
        lInput, /*matchedById=*/false, nullptr, OpaaxString("Game/Assets/Unlisted.png"));
    CHECK(lId == OpaaxStringID(OpaaxString("Game/Assets/Unlisted.png")));
    CHECK(lId.ToString() == "Game/Assets/Unlisted.png");
}

TEST_CASE("ResolveCanonicalAssetId: an ID hit takes precedence over a path hit")
{
    const OpaaxStringID lInput  = OPAAX_ID("Textures/Player");
    const OpaaxStringID lPathId = OPAAX_ID("Textures/Other");
    const OpaaxStringID lId     = ResolveCanonicalAssetId(
        lInput, /*matchedById=*/true, &lPathId, OpaaxString("rel/path.png"));
    CHECK(lId == lInput); // id-hit wins; path-hit ignored
}
