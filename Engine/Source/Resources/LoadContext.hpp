#pragma once

#include "Core/OpaaxTypes.h"

#include "Resources/ResourceConcept.hpp"
#include "Resources/ResourceRef.hpp"
#include "Resources/ResourceDependencyGraph.hpp"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcePool.hpp"

// =============================================================================
// LoadContext — loading with dependencies (Level -> Textures).
//   A loader acquires its dependencies through it: they stay loaded while the parent
//   holds them, and each one is recorded in the dependency graph. A path that is already
//   loading is a cycle: Acquire fails.
//   Acquire<TSub> is defined in ResourceManager.h (needs the complete manager).
// =============================================================================
namespace Opaax
{
    class ResourceManager;

    class LoadContext final
    {
        // =============================================================================
        // CTORS
        // =============================================================================
    public:
        // Sync: each slot is published as it loads.
        // Async (bDeferred): slots stay Loading and are published on the main thread by PublishAll().
        LoadContext(ResourceManager& InManager, ResourceDependencyGraph& InDeps, bool bInDeferred = false) noexcept
            : m_Manager(InManager)
            , m_Deps(InDeps)
            , m_Deferred(bInDeferred)
        {
        }

        LoadContext(const LoadContext&)            = delete;
        LoadContext& operator=(const LoadContext&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Loads a dependency. The returned Ref keeps it alive while the parent holds it.
         * A path that is already loading (cycle) returns an invalid ref, and the parent's Load fails.
         */
        template<CResource TSub>
        ResourceRef<TSub> Acquire(const char* InPath); // defined in ResourceManager.h
        
        /**
         * Marks a path as loading.
         * @return False if it is already loading (cycle)
         */
        bool PushLoading(Uint32 InId)
        {
            if (IsLoading(InId))
            {
                return false;
            }
            
            m_Chain.emplace_back(InId);
            return true;
        }

        void PopLoading() noexcept
        {
            if (!m_Chain.empty())
            {
                m_Chain.pop_back();
            }
        }

        bool IsLoading(Uint32 InId) const noexcept
        {
            for (const Uint32 lId : m_Chain)
            {
                if (lId == InId)
                {
                    return true;
                }
            }
            
            return false;
        }

        /**
         * @return The resource currently loading (dependency parent), or None at the root
         */
        Uint32 CurrentParent() const noexcept
        {
            return m_Chain.empty() ? OpaaxGlobal::ID_None : m_Chain.back();
        }

        // Deferred publish (async)
        bool IsDeferred() const noexcept { return m_Deferred; }

        // Records a filled Loading slot to publish on the main thread. Children come before
        // their parent, so dependencies are published first.
        void AddPendingInit(IResourcePool* InPool, Uint32 InSlot)
        {
            m_PendingInit.emplace_back(InPool, InSlot);
        }

        // Main thread: publishes every recorded slot, children first. The caller holds the
        // manager lock. Clears the list.
        void PublishAll()
        {
            for (const PendingInit& lEntry : m_PendingInit)
            {
                lEntry.Pool->FinalizeSlot(lEntry.Slot);
            }
            m_PendingInit.clear();
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // A filled slot waiting to be published (async).
        struct PendingInit
        {
            IResourcePool* Pool;
            Uint32         Slot;
        };

        ResourceManager&         m_Manager;
        ResourceDependencyGraph& m_Deps;
        TDynArray<Uint32>        m_Chain;       // path ids currently loading
        TDynArray<PendingInit>   m_PendingInit; // async: slots waiting to be published
        bool                     m_Deferred;    // async: publish on the main thread
    };
}
