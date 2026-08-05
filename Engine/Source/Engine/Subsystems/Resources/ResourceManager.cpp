#include "ResourceManager.h"

namespace Opaax
{
    // =========================================================================
    // Lifecycle — Startup performs ZERO disk IO and ZERO GPU work (pools are
    // lazy), which is exactly what lets Resources be the first engine subsystem.
    // =========================================================================
    ResourceManager::~ResourceManager()
    {
        // Unload while pools + manager are still alive (child-Ref dtors call back in).
        FlushAll();
    }

    bool ResourceManager::Startup()
    {
        OPAAX_LOG(LogResourceManager, Info, "ResourceManager startup — pools created on first Load<T>");
        return true;
    }

    void ResourceManager::Shutdown()
    {
        FlushAll();
        OPAAX_LOG(LogResourceManager, Info, "ResourceManager shutdown");
    }

    // =========================================================================
    // The pump — the single point where pool mutation is finalised. Async payloads
    // published this frame land via the job system's completion drain (the engine
    // loop's job, not ours); here we sweep the graveyard, destroying every slot
    // released since the previous pump. That deferral is the frame-stable guarantee:
    // a pointer returned by Resolve() stays valid until the next Update().
    // =========================================================================
    void ResourceManager::Update(double /*InDeltaTime*/)
    {
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            ++m_PumpEpoch; // advance BEFORE collecting: a view from last frame is now stale
            for (TUniquePtr<IResourcePool>& lPool : m_Pools)
            {
                if (lPool)
                {
                    lPool->CollectGarbage(); // may cascade-release composites -> Release re-locks (recursive)
                }
            }
        }
        FirePendingCallbacks(); // deliver LoadAsync completions (user code runs outside the lock)
    }

    // Poll each pending completion outside the lock (user callbacks may re-enter LoadAsync
    // or do work). A closure returns true once it has fired; keep the still-loading ones,
    // merging back any callbacks registered during firing.
    void ResourceManager::FirePendingCallbacks()
    {
        TDynArray<TFunction<bool()>> lBatch;
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            if (m_PendingCallbacks.empty()) { return; }
            lBatch.swap(m_PendingCallbacks);
        }

        TDynArray<TFunction<bool()>> lStillPending;
        for (TFunction<bool()>& lPoll : lBatch)
        {
            if (!lPoll()) { lStillPending.push_back(Move(lPoll)); } // still Loading -> keep
        }

        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        for (TFunction<bool()>& lNew : m_PendingCallbacks) { lStillPending.push_back(Move(lNew)); }
        m_PendingCallbacks.swap(lStillPending);
    }

    void ResourceManager::FlushAll()
    {
        // NOTE: composite payloads release their child refs as they unload, so a
        // pool holding composites should ideally flush before the pools it depends
        // on. Flushing in creation order relies on the leak warning to surface
        // anything still referenced — good enough for the current type set.
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        // Drop pending completions FIRST — each holds an internal claim; releasing them
        // before UnloadAll keeps in-flight async loads from tripping the leak warning.
        m_PendingCallbacks.clear();
        for (TUniquePtr<IResourcePool>& lPool : m_Pools)
        {
            if (lPool)
            {
                lPool->UnloadAll();
            }
        }
        m_Deps.Clear();
    }

    void ResourceManager::AddDependencyEdge(Uint32 InParentId, Uint32 InChildId)
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        m_Deps.AddEdge(InParentId, InChildId);
    }
}
