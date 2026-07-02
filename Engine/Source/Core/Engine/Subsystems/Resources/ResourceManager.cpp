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
        OPAAX_CORE_INFO("[Resources] ResourceManager startup — pools created on first Load<T>");
        return true;
    }

    void ResourceManager::Shutdown()
    {
        FlushAll();
        OPAAX_LOG(LogResourceManager, Info, "ResourceManager shutdown")
    }

    // =========================================================================
    // The pump — the single point where pool mutation is allowed to happen.
    // M-RES-1: loads/unloads are immediate (main-thread, synchronous), so this is
    // a near no-op that DOCUMENTS the frame-stable boundary: a pointer returned by
    // Resolve() is valid until the next Update(). It will host M-RES-2 async
    // finalize + deferred unload + graveyard, and M-RES-3 hot-reload swaps.
    // =========================================================================
    void ResourceManager::Update(double /*InDeltaTime*/)
    {
    }

    void ResourceManager::FlushAll()
    {
        // NOTE: composite payloads release their child refs as they unload, so a
        // pool holding composites should ideally flush before the pools it depends
        // on. M-RES-1 flushes in creation order and relies on the leak warning to
        // surface anything still referenced — good enough pre-async.
        for (UniquePtr<IResourcePool>& lPool : m_Pools)
        {
            if (lPool)
            {
                lPool->UnloadAll();
            }
        }
        m_Deps.Clear();
    }
}
