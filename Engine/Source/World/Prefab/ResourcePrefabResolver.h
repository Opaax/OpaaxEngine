#pragma once

#include "Core/EngineAPI.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before PrefabResource — completes LoadContext

#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabResource.hpp"

namespace Opaax
{
    class IPaths;

    // =============================================================================
    // ResourcePrefabResolver — the ONE real IPrefabResolver: asset path -> loaded prefab.
    //
    //   It is what closes the gap `IPrefabResolver` exists to name: resolving a path needs `IPaths`
    //   (an app service) and the `ResourceManager` (an engine subsystem), and both of its callers
    //   already hold the pair — `Level` by construction (**WM8**), and the editor through its
    //   context. So neither has to reach for anything, and the World layer still names no service.
    //
    //   IT HOLDS THE CLAIMS, and that is not caching for speed — it is LIFETIME. `Resolve` answers a
    //   pointer INTO the resource pool, and that payload is only alive while some `ResourceRef`
    //   holds it. Dropping the ref at the end of `Resolve` would return a pointer to something the
    //   pool may already have unloaded. Keep the resolver alive for as long as its answers are used,
    //   which for a fold or an expand is one call.
    //
    //   The dedup is a real second benefit: a map with forty placements of one prefab loads it once,
    //   and the ResourceManager would have deduped anyway — this just skips forty lookups.
    // =============================================================================
    class OPAAX_API ResourcePrefabResolver final : public IPrefabResolver
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        ResourcePrefabResolver(const IPaths& InPaths, ResourceManager& InResources) noexcept
            : m_Paths(InPaths)
            , m_Resources(InResources)
        {
        }

        ResourcePrefabResolver(const ResourcePrefabResolver&)            = delete;
        ResourcePrefabResolver& operator=(const ResourcePrefabResolver&) = delete;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IPrefabResolver interface
        /**
         * @return the prefab's entities, or NULL when the path is empty, resolves outside the asset
         *   trees, or fails to load. FailFast makes that last case a null rather than an empty
         *   prefab, which is exactly the distinction the fold needs to keep data.
         */
        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override;
        //~End IPrefabResolver interface

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct Claim
        {
            OpaaxString                 Path;
            ResourceRef<PrefabResource> Ref;   // KEEPS the payload alive — see the header note
        };

        const IPaths&           m_Paths;
        ResourceManager&        m_Resources;

        // Mutable because Resolve is logically const — it answers a question — while physically
        // needing to record the claim that makes its own answer valid.
        mutable TDynArray<Claim> m_Claims;
    };
}
