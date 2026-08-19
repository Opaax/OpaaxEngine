#pragma once

#include "Core/OpaaxTypes.h"

#include "ResourceConcept.hpp"
#include "ResourceRef.hpp"
#include "ResourceDependencyGraph.hpp"
#include "ResourceManager.h"
#include "ResourcePool.hpp"

// =============================================================================
// LoadContext — composite loading (Level -> Textures), refcounts chain.
//
//   A loader receives LoadContext& and Acquire()s its hard dependencies through
//   it: refs chain (releasing the parent releases them), and every Acquire records
//   a forward+reverse edge in the dependency graph. The context carries the
//   in-flight load chain, so a path already loading is a HARD CYCLE — Acquire
//   fails it loudly. The hard-reference graph is therefore a DAG by construction.
//
//   Templated Acquire<TSub> is declared here and DEFINED in ResourceManager.h
//   (needs the complete manager) — same cycle-break as ResourceRef.
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
        // bDeferred=false (sync): the manager publishes each slot inline as it loads.
        // bDeferred=true (async): loads leave slots Loading + record them here for a
        // single main-thread PublishAll() at the pump — Initialize() stays off the worker.
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
         * Acquire a hard sub-dependency. Recursion into the manager; the returned Ref keeps it alive as long as the parent payload holds it. 
         * A path already in the in-flight chain returns an INVALID ref (cycle) — the parent's Load then fails.
         * @tparam TSub 
         * @param InPath 
         * @return 
         */
        template<CResource TSub>
        ResourceRef<TSub> Acquire(const char* InPath); // defined in ResourceManager.h
        
        /**
         * Push a path id entering its load; false if it is already loading (a cycle).
         * @param InId 
         * @return 
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

        /**
         * 
         */
        void PopLoading() noexcept
        {
            if (!m_Chain.empty())
            {
                m_Chain.pop_back();
            }
        }

        /***/
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
         * @return The resource whose Load is currently running (edge parent) — None at root.
         */
        Uint32 CurrentParent() const noexcept
        {
            return m_Chain.empty() ? OpaaxGlobal::ID_None : m_Chain.back();
        }

        // ----- deferred publish (async) -----------------------------------------
        bool IsDeferred() const noexcept { return m_Deferred; }

        // Record a freshly-filled Loading slot to publish at the pump. Accumulated in
        // load order (post-order: children before their parent), so PublishAll finalizes
        // dependencies first. Runs on the loading thread; the list is context-local.
        void AddPendingInit(IResourcePool* InPool, Uint32 InSlot)
        {
            m_PendingInit.emplace_back(InPool, InSlot);
        }

        // Main-thread: publish every recorded slot (Initialize + flip Loaded), children
        // first. The caller holds the manager lock. Idempotent — clears the list.
        void PublishAll()
        {
            for (const PendingInit& lEntry : m_PendingInit)
            {
                lEntry.Pool->FinalizeSlot(lEntry.Slot);
            }
            m_PendingInit.clear();
        }

        // =============================================================================
        // Getter
    public:
        ResourceManager&         GetManager() const noexcept { return m_Manager; }
        ResourceDependencyGraph& GetDeps()    const noexcept { return m_Deps;    }
        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        // A filled-but-not-yet-published slot (async): FinalizeSlot(Slot) on Pool at the pump.
        struct PendingInit
        {
            IResourcePool* Pool;
            Uint32         Slot;
        };

        ResourceManager&         m_Manager;
        ResourceDependencyGraph& m_Deps;
        TDynArray<Uint32>        m_Chain;       // interned path ids currently in-flight
        TDynArray<PendingInit>   m_PendingInit; // async: slots awaiting main-thread publish
        bool                     m_Deferred;    // async load -> defer publish to the pump
    };
}
