#pragma once

#include "Core/EngineAPI.h"
#include "Resources/ResourceManager.h"   // before PrefabResource (completes LoadContext)

#include "World/Prefab/PrefabFold.h"
#include "World/Prefab/PrefabResource.hpp"

namespace Opaax
{
    class ComponentRegistry;
    class IPaths;

    // =============================================================================
    // ResourcePrefabResolver — IPrefabResolver using IPaths and the ResourceManager.
    //   Keeps the loaded prefabs alive: Resolve returns pointers into them, so keep the resolver
    //   alive while you use its results.
    // =============================================================================
    class ResourcePrefabResolver final : public IPrefabResolver
    {
        // =========================================================================
        // CTORS - DTORS
        // =========================================================================
    public:
        ResourcePrefabResolver(const IPaths& InPaths, ResourceManager& InResources,
                               const ComponentRegistry& InComponents) noexcept
            : m_Paths(InPaths)
            , m_Resources(InResources)
            , m_Components(InComponents)
        {
        }

        ResourcePrefabResolver(const ResourcePrefabResolver&)            = delete;
        ResourcePrefabResolver& operator=(const ResourcePrefabResolver&) = delete;

        // =========================================================================
        // Override
        // =========================================================================
        //~Begin IPrefabResolver interface
        /**
         * @return The flattened prefab, or null if the path is empty, cannot be loaded, or is
         *   already being flattened (a cycle, reported as an error)
         */
        const PrefabData* Resolve(const OpaaxString& InAssetPath) const override;
        //~End IPrefabResolver interface

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /**
         * True if InOuter places InInner, at any depth. False if a path does not resolve.
         */
        bool Places(const OpaaxString& InOuter, const OpaaxString& InInner) const;

        // =========================================================================
        // Members
        // =========================================================================
    private:
        struct Claim
        {
            OpaaxString                 Path;
            ResourceRef<PrefabResource> Ref;        // keeps the payload loaded
            TUniquePtr<PrefabData>      Flattened;  // what Resolve returns; null for a failed load
            bool                        bInFlight = false;   // cycle guard
        };

        /** The entry for InAssetPath, loaded and flattened on first use. */
        Claim& ClaimFor(const OpaaxString& InAssetPath) const;

        const IPaths&            m_Paths;
        ResourceManager&         m_Resources;
        const ComponentRegistry& m_Components;

        // Mutable: Resolve is logically const but keeps what it loaded. On the heap, so pointers
        // from Resolve(A) survive Resolve(B).
        mutable TDynArray<TUniquePtr<Claim>> m_Claims;
    };
}
