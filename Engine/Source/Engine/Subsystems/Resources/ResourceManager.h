#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Engine/Subsystems/EngineSubsystem.h"
#include "Application/Services/IJobSystem.h"

#include "ResourceTypeID.hpp"
#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"
#include "ResourceDependencyGraph.hpp"
#include "ResourcePool.hpp"
#include "ResourceRef.hpp"
#include "LoadContext.hpp"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// Load Resource
// Sync:
// ResourceRef<MyResourceType> MyResource = m_Resources->Load<MyResourceType>(ResoucePath);
// Async 1:
// ResourceRef<MyResourceType> MyResource = m_Resources->LoadAsync<MyResourceType>(ResoucePath);
// if(MyResource.IsValid()){ Do thing } 
//
// Async 2:
// m_Resources->LoadAsync<MyResourceType>(ResoucePath, [this](LoadAsyncResult<MyResourceType> LoadedResource)
// {
//      if (LoadedResource.bFailed) { fail... }
//      else{ MyResource (from this) = LoadedResource.Ref;}
// });
// if(MyResource.IsValid()){ Do thing } 
// 
// ================================ END USAGE ==================================
// =============================================================================

// =============================================================================
// ResourceManager — routing. The first engine subsystem, a thin core service.
//
//   Owns one ResourcePool<T> per type (lazy-created, indexed by ResourceTypeID),
//   plus the dependency graph. Public surface is FROZEN (review C5):
//       Load / Resolve / Pin / FlushAll / Update
//   Every future capability (hot reload, cooking, streaming, editor type info) is
//   a separate system CONSUMING this API — never a manager feature.
//
//   This header is the umbrella: the crossing template bodies (ResourceRef<T>,
//   LoadContext::Acquire, ResourceManager::Load...) are defined at the bottom,
//   where the manager, the pools, the refs and the context are all complete —
//   which is what breaks the Ref/Context <-> Manager template dependency cycle.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourceManager{"ResourceManager"};

    // =============================================================================
    // LoadAsyncResult<T> — delivered to a LoadAsync completion callback, once, on the
    // pump. Ref is the loaded claim (keep it to retain the resource); on failure it is
    // empty, Status is Failed, and bFailed is true.
    // =============================================================================
    template<typename T>
    struct LoadAsyncResult
    {
        ResourceRef<T> Ref;
        EResourceState Status  = EResourceState::Failed;
        bool           bFailed = true;
    };

    class OPAAX_API ResourceManager final : public EngineSubsystemBase
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SUBSYSTEM_TYPE(ResourceManager)

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /***/
        ResourceManager() = default;
        
        /**
         * NOTE: FlushAll() in the body (see .cpp) — composite payloads hold child Refs
         * whose dtors call back into the manager, so everything must unload while the manager + pools are still alive ("flush before context death").
         */
        ~ResourceManager() override;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        ResourceManager(const ResourceManager&)            = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&)                 = delete;
        ResourceManager& operator=(ResourceManager&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Load (or dedup-share) a resource; returns an owning RAII claim.
         * @tparam T 
         * @param InPath 
         * @return 
         */
        template<CResource T>
        ResourceRef<T> Load(const char* InPath);

        /**
         * Kick an asynchronous load: returns a valid Loading claim immediately (Resolve
         * yields the placeholder until it publishes). File IO + decode run on a worker
         * (whole subtree, one job); Initialize + flip Loaded happen on the pump. Falls
         * back to inline load when no real job system is set (the null object).
         *
         * InOnComplete (optional) fires ONCE on the pump when the load settles — Loaded
         * or Failed, and immediately-next-pump for an already-loaded path. While it is
         * pending the manager holds an internal claim, so the resource survives to the
         * callback even if you drop the returned Ref: keep result.Ref to retain it.
         * @tparam T
         * @param InPath
         * @param InOnComplete
         * @return
         */
        template<CResource T>
        ResourceRef<T> LoadAsync(const char* InPath, TFunction<void(LoadAsyncResult<T>)> InOnComplete = {});
        
        /**
         * Frame-stable view. O(1). Never null for Placeholder-policy types
         * Null for FailFast on stale/invalid. NEVER cache across a Resources.Update() pump.
         * @tparam T 
         * @param InHandle 
         * @return 
         */
        template<CResource T>
        T* Resolve(ResourceHandle<T> InHandle) noexcept;
        
        /**
         * Bridge a data Handle to a lifetime claim (adds a ref). 
         * Empty ref if the handle is stale/dead — Pin cannot resurrect an unloaded resource.
         * @tparam T 
         * @param InHandle 
         * @return 
         */
        template<CResource T>
        ResourceRef<T> Pin(ResourceHandle<T> InHandle);
        
        /**
         * Unload every resource in every pool (warns on leaks). 
         * Flush BEFORE the GPU context dies; destroy the manager after everything else (boot/shutdown order).
         */
        void FlushAll();

        /**
         * Inject the worker pool used by LoadAsync (the Engine wires this from the app service locator at Startup). Defaults to the null job system (inline loads).
         * @param InJobs 
         */
        void SetJobSystem(IJobSystem& InJobs) noexcept { m_Jobs = &InJobs; }
        
        /**
         * Monotonic pump counter (bumped each Update). A CheckedView snapshots it and flags itself stale once it advances — catches Resolve pointers held across a pump.
         * @return 
         */
        Uint64 GetPumpEpoch() const noexcept { return m_PumpEpoch; }

    public:
        /***/
        template<CResource T> Uint32 GetLoadedCount();
        /***/
        template<CResource T> Uint32 GetLoadingCount();
        /***/
        template<CResource T> Uint64 GetBytes();
        // Current load state of a handle (Unloaded if stale/never-loaded). Drives the
        // async completion callbacks (Loading -> keep polling; else fire).
        template<CResource T> EResourceState GetState(ResourceHandle<T> InHandle);

    public:
        /***/
        template<CResource T> void AddRef(ResourceHandle<T> InHandle) noexcept;
        /***/
        template<CResource T> void Release(ResourceHandle<T> InHandle) noexcept;

        /**
         * Recursion target for composite loads: shares the in-flight LoadContext so the load chain (and cycle detection) accumulates across nested acquisitions.
         * @tparam T
         * @param InPath
         * @param InCtx
         * @return
         */
        template<CResource T>
        ResourceRef<T> LoadInternal(const char* InPath, LoadContext& InCtx);
        
        /**
         * Record a hard-dependency edge (parent -> child) under the manager lock.
         * Called by LoadContext::Acquire, which may run on a worker while loading a composite.
         * @param InParentId 
         * @param InChildId 
         */
        void AddDependencyEdge(Uint32 InParentId, Uint32 InChildId);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        template<CResource T>
        ResourcePool<T>& GetOrCreatePool();

        // Poll every registered completion callback (main thread, on the pump): fire +
        // drop the ones whose resource has settled, keep the still-loading ones. User
        // callbacks run OUTSIDE the lock (they may re-enter LoadAsync / do work).
        void FirePendingCallbacks();

        // =============================================================================
        // Override
        // =============================================================================
        
        //~Begin ISubsystem interface
    public:
        bool Startup() override;             // builds nothing — pools are lazy
        void Shutdown() override;            // FlushAll + leak report
        void Update(double DeltaTime) override; // the pump (single mutation point)
        //~End ISubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * One coarse recursive lock guards ALL pool + registry + graph mutation.
         * Recursive because unloading a composite cascade-releases its children on the same thread (Release re-enters).
         * NEVER held across T::Load (the off-thread heavy work) so a worker's decode can't stall the main thread's Resolve — uncontended in steady state.
         */
        mutable RecursiveMutex              m_Mutex;
        TDynArray<UniquePtr<IResourcePool>> m_Pools; // indexed by ResourceTypeID::Get<T>()
        ResourceDependencyGraph             m_Deps;
        IJobSystem*                         m_Jobs = &IJobSystem::Null(); // LoadAsync worker pool
        Uint64                              m_PumpEpoch = 0; // ++ each Update (CheckedView staleness)

        // Pending LoadAsync completion callbacks. Each poll closure holds the internal
        // claim + the user callback; returns true once it has fired (Loaded/Failed).
        TDynArray<TFunction<bool()>>        m_PendingCallbacks;
    };

    // =============================================================================
    // ResourceAsyncLoad — per-request state shared between the worker (fill) and the
    // main-thread drain (publish). Heap-owned via SharedPtr captured by both lambdas;
    // owns the path string + the LoadContext across the thread boundary.
    // =============================================================================
    struct ResourceAsyncLoad
    {
        OpaaxString            Path;
        UniquePtr<LoadContext> Ctx;
        bool                   Filled = false;
    };

    // =============================================================================
    // ResourceManager — template definitions (all types complete below this line)
    // =============================================================================
    /***/
    template<CResource T>
    ResourcePool<T>& ResourceManager::GetOrCreatePool()
    {
        const Uint32 lId = ResourceTypeID::Get<T>();
        if (lId >= m_Pools.size()) { m_Pools.resize(lId + 1); }
        if (!m_Pools[lId])         { m_Pools[lId] = MakeUnique<ResourcePool<T>>(); }
        // NOTE: safe — the slot at lId is only ever populated with a ResourcePool<T>.
        return *static_cast<ResourcePool<T>*>(m_Pools[lId].get());
    }
    
    /**
     * Load orchestration shared by sync Load and every composite Acquire: 
     * AcquireSlot (locked) -> FillSlot (UNLOCKED — runs T::Load, child Acquires recurse here) -> publish (locked; inline for sync, deferred to the pump for async).
     * @tparam T 
     * @param InPath 
     * @param InCtx 
     * @return 
     */
    template<CResource T>
    ResourceRef<T> ResourceManager::LoadInternal(const char* InPath, LoadContext& InCtx)
    {
        const OpaaxStringID lId(InPath);
        if (!InCtx.PushLoading(lId.GetId())) // cycle guard — context-local, no lock needed
        {
            OPAAX_LOG(LogResourceManager, Error, "Hard dependency cycle on '{}'", InPath)
            return ResourceRef<T>{ this, ResourceHandle<T>{} };
        }

        bool              lNeedsFill = false;
        ResourcePool<T>*  lPool      = nullptr;
        ResourceHandle<T> lHandle{};
        {
            LockGuard<RecursiveMutex> lLock(m_Mutex);
            lPool   = &GetOrCreatePool<T>();
            lHandle = lPool->AcquireSlot(InPath, lNeedsFill);
        }

        if (lNeedsFill)
        {
            const bool lOk = lPool->FillSlot(lHandle, InPath, InCtx); // T::Load — UNLOCKED

            LockGuard<RecursiveMutex> lLock(m_Mutex);
            if (!lOk)
            {
                lPool->AbandonSlot(lHandle.Slot);
                InCtx.PopLoading();
                return ResourceRef<T>{ this, ResourceHandle<T>{} };
            }
            
            if (InCtx.IsDeferred())
            {
                InCtx.AddPendingInit(lPool, lHandle.Slot);
            } 
            // publish at the pump
            else
            {
                lPool->FinalizeSlot(lHandle.Slot);
            }         // publish inline (main thread)
        }

        InCtx.PopLoading();
        return ResourceRef<T>{ this, lHandle }; // adopt the +1 AcquireSlot applied
    }

    /***/
    template<CResource T>
    ResourceRef<T> ResourceManager::Load(const char* InPath)
    {
        LoadContext lCtx(*this, m_Deps); // synchronous -> LoadInternal publishes inline
        return LoadInternal<T>(InPath, lCtx);
    }

    /***/
    template<CResource T>
    ResourceRef<T> ResourceManager::LoadAsync(const char* InPath, TFunction<void(LoadAsyncResult<T>)> InOnComplete)
    {
        ResourcePool<T>*  lPool      = nullptr;
        bool              lNeedsFill = false;
        ResourceHandle<T> lHandle{};
        {
            LockGuard<RecursiveMutex> lLock(m_Mutex);
            lPool   = &GetOrCreatePool<T>();
            lHandle = lPool->AcquireSlot(InPath, lNeedsFill); // Loading claim, returned NOW
        }

        if (lNeedsFill)
        {
            // Fill the whole subtree on one worker; publish (Initialize + Loaded) at the
            // pump. With the null job system this runs inline (work + onComplete here).
            SharedPtr<ResourceAsyncLoad> lJob = MakeShared<ResourceAsyncLoad>();
            lJob->Path             = InPath; // own the string across the thread boundary
            ResourceManager* lSelf = this;

            m_Jobs->Submit(
                [lSelf, lPool, lHandle, lJob]() // WORKER — no lock across T::Load
                {
                    lJob->Ctx    = MakeUnique<LoadContext>(*lSelf, lSelf->m_Deps, /*deferred*/ true);
                    lJob->Filled = lPool->FillSlot(lHandle, lJob->Path.CStr(), *lJob->Ctx);
                    if (lJob->Filled)
                    {
                        lJob->Ctx->AddPendingInit(lPool, lHandle.Slot);
                    } // root, after its children
                },
                [lSelf, lPool, lHandle, lJob]() // MAIN (drain) — publish children-first, else abandon
                {
                    LockGuard<RecursiveMutex> lLock(lSelf->m_Mutex);
                    if (lJob->Ctx)
                    {
                        lJob->Ctx->PublishAll();
                    }

                    if (!lJob->Filled)
                    {
                        lPool->AbandonSlot(lHandle.Slot);
                    }
                });
        }

        ResourceRef<T> lRef{ this, lHandle }; // adopt the +1 AcquireSlot applied (fresh OR dedup)

        // Optional completion: an internal claim (copy) keeps the resource alive until the
        // callback fires on a pump — so a fire-and-forget caller need not hold the Ref.
        if (InOnComplete)
        {
            ResourceRef<T>   lClaim = lRef;
            ResourceManager* lSelf  = this;
            TFunction<bool()> lPoll =
                [lSelf, lClaim = Move(lClaim), lCb = Move(InOnComplete)]() -> bool
                {
                    const EResourceState lState = lSelf->GetState<T>(lClaim.GetHandle());
                    if (lState == EResourceState::Loading) { return false; } // not settled — poll again

                    LoadAsyncResult<T> lResult;
                    lResult.Status  = lState;
                    lResult.bFailed = (lState != EResourceState::Loaded);
                    if (!lResult.bFailed) { lResult.Ref = lClaim; }
                    lCb(Move(lResult));
                    return true; // fired -> drop; the internal claim releases here
                };

            LockGuard<RecursiveMutex> lLock(m_Mutex);
            m_PendingCallbacks.push_back(Move(lPoll));
        }

        return lRef;
    }

    /***/
    template<CResource T>
    T* ResourceManager::Resolve(ResourceHandle<T> InHandle) noexcept
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().Get(InHandle);
    }

    /***/
    template<CResource T>
    ResourceRef<T> ResourceManager::Pin(ResourceHandle<T> InHandle)
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        ResourcePool<T>& lPool = GetOrCreatePool<T>();
        if (!lPool.IsLive(InHandle))
        {
            return ResourceRef<T>{};
        }
        
        lPool.AddRef(InHandle);
        return ResourceRef<T>{ this, InHandle };
    }

    /***/
    template<CResource T>
    void ResourceManager::AddRef(ResourceHandle<T> InHandle) noexcept
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        GetOrCreatePool<T>().AddRef(InHandle);
    }

    /***/
    template<CResource T>
    void ResourceManager::Release(ResourceHandle<T> InHandle) noexcept
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        GetOrCreatePool<T>().Release(InHandle);
    }

    /***/
    template<CResource T>
    Uint32 ResourceManager::GetLoadedCount()
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetLoadedCount();
    }

    /***/
    template<CResource T>
    Uint32 ResourceManager::GetLoadingCount()
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetLoadingCount();
    }

    /***/
    template<CResource T>
    Uint64 ResourceManager::GetBytes()
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetBytes();
    }

    /***/
    template<CResource T>
    EResourceState ResourceManager::GetState(ResourceHandle<T> InHandle)
    {
        LockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetState(InHandle);
    }

    // =============================================================================
    // ResourceRef<T> — crossing bodies (manager complete)
    // =============================================================================
    template<typename T>
    ResourceRef<T>::ResourceRef(const ResourceRef& InOther)
        : m_Manager(InOther.m_Manager)
        , m_Handle(InOther.m_Handle)
    {
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->AddRef(m_Handle);
        }
    }

    template<typename T>
    ResourceRef<T>& ResourceRef<T>::operator=(const ResourceRef& InOther)
    {
        if (this == &InOther)
        {
            return *this;
        }
        
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->Release(m_Handle);
        }
        
        m_Manager = InOther.m_Manager;
        m_Handle  = InOther.m_Handle;
        
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->AddRef(m_Handle);
        }
        
        return *this;
    }

    template<typename T>
    ResourceRef<T>& ResourceRef<T>::operator=(ResourceRef&& InOther) noexcept
    {
        if (this == &InOther)
        {
            return *this;
        }
        
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->Release(m_Handle);
        }
        
        m_Manager = InOther.m_Manager;
        m_Handle  = InOther.m_Handle;
        InOther.m_Manager = nullptr;
        InOther.m_Handle  = ResourceHandle<T>{};
        
        return *this;
    }

    /***/
    template<typename T>
    ResourceRef<T>::~ResourceRef()
    {
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->Release(m_Handle);
        }
    }

    /***/
    template<typename T>
    T* ResourceRef<T>::Get() const noexcept
    {
        return (m_Manager != nullptr) ? m_Manager->Resolve(m_Handle) : nullptr;
    }

    // =============================================================================
    // LoadContext::Acquire — crossing body (manager complete)
    // =============================================================================
    
    /***/
    template<CResource TSub>
    ResourceRef<TSub> LoadContext::Acquire(const char* InPath)
    {
        const OpaaxStringID lChild(InPath);

        // Cycle: acquiring something already loading up the chain. Fail loudly, do NOT
        // record an edge (keeps the hard-reference graph a DAG).
        if (IsLoading(lChild.GetId()))
        {
            OPAAX_LOG(LogResourceManager, Error, "Hard dependency cycle on '{}'", InPath)
            return ResourceRef<TSub>{ &m_Manager, ResourceHandle<TSub>{} };
        }

        const Uint32 lParent = CurrentParent();
        if (lParent != OpaaxGlobal::ID_None)
        {
            m_Manager.AddDependencyEdge(lParent, lChild.GetId()); // locked (may run on a worker)
        }

        return m_Manager.LoadInternal<TSub>(InPath, *this);
    }
}
