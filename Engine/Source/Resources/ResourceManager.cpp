#include "ResourceManager.h"

namespace Opaax
{
    // =========================================================================
    // Lifecycle — Startup does no IO and no GPU work (pools are created on demand).
    // =========================================================================
    ResourceManager::~ResourceManager()
    {
        // Unload while the pools and the manager are still alive.
        FlushAll();
    }

    bool ResourceManager::Startup()
    {
        return true;
    }

    void ResourceManager::Shutdown()
    {
        FlushAll();
    }

    // =========================================================================
    // Update — destroys the slots released since the last Update. Deferring this is what
    //   keeps pointers returned by Resolve() valid until the next Update().
    // =========================================================================
    void ResourceManager::Update(double /*InDeltaTime*/)
    {
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            ++m_PumpEpoch; // before collecting: last frame's views are now stale
            for (TUniquePtr<IResourcePool>& lPool : m_Pools)
            {
                if (lPool)
                {
                    lPool->CollectGarbage(); // may release children (re-locks)
                }
            }
        }
        FirePendingCallbacks(); // LoadAsync callbacks, outside the lock
    }

    // Poll pending completions outside the lock (callbacks may call LoadAsync).
    // Keep the ones still loading, plus any registered during firing.
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
            if (!lPoll()) { lStillPending.emplace_back(Move(lPoll)); } // still loading
        }

        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        for (TFunction<bool()>& lNew : m_PendingCallbacks) { lStillPending.emplace_back(Move(lNew)); }
        m_PendingCallbacks.swap(lStillPending);
    }

    void ResourceManager::FlushAll()
    {
        // Pools are flushed in creation order; the leak warning reports anything still referenced.
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        // Drop pending completions first (each holds a claim), so async loads do not report leaks.
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
