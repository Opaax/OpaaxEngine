#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/OpaaxGlobal.h"

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
// Sync:
//   ResourceRef<MyResourceType> MyResource = m_Resources->Load<MyResourceType>(Path);
// Async 1:
//   ResourceRef<MyResourceType> MyResource = m_Resources->LoadAsync<MyResourceType>(Path);
//   if (MyResource.IsValid()) { ... }
// Async 2:
//   m_Resources->LoadAsync<MyResourceType>(Path, [this](LoadAsyncResult<MyResourceType> InResult)
//   {
//       if (InResult.bFailed) { ... }
//       else { MyResource = InResult.Ref; }
//   });
// 
// ================================ END USAGE ==================================
// =============================================================================

// =============================================================================
// ResourceManager — loads and caches resources. One ResourcePool<T> per type
//   (created on demand) plus the dependency graph.
//   Hot reload, cooking, etc. should be separate systems using this API.
//   Template bodies are defined at the bottom of this header, where every type is complete.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourceManager{"ResourceManager"};

    // =============================================================================
    // LoadAsyncResult<T> — passed once to a LoadAsync callback. Keep Ref to keep the resource.
    //   On failure Ref is empty and bFailed is true.
    // =============================================================================
    template<typename T>
    struct LoadAsyncResult
    {
        ResourceRef<T> Ref;
        EResourceState Status  = EResourceState::Failed;
        bool           bFailed = true;
    };

    class ResourceManager final : public EngineSubsystemBase
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
        ResourceManager() = default;
        
        /**
         * Calls FlushAll(): children must be unloaded while the manager and pools are alive.
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
         * Loads a resource, or shares it if already loaded.
         * @return A claim that keeps it loaded
         */
        template<CResource T>
        ResourceRef<T> Load(const char* InPath);

        /**
         * Starts an async load and returns a Loading claim immediately (resolves to the placeholder
         * until published). Loads on a worker; Initialize runs on the main thread in Update.
         * Loads inline when there is no job system.
         *
         * InOnComplete (optional) is called once in Update when the load succeeds or fails,
         * and the resource stays loaded until then. Keep InResult.Ref to keep it.
         */
        template<CResource T>
        ResourceRef<T> LoadAsync(const char* InPath, TFunction<void(LoadAsyncResult<T>)> InOnComplete = {});

        /**
         * A claim on InPath only if it is already loaded; never loads (e.g. for an asset browser).
         * @return An empty ref if unknown or still loading
         */
        template<CResource T>
        ResourceRef<T> Find(const char* InPath);

        /**
         * Reloads a resource that is already loaded, in place. Existing refs see the new data.
         * Main thread only (runs Initialize).
         * @return False if not loaded, still loading, or the reload failed (the old data is kept)
         */
        template<CResource T>
        bool Reload(const char* InPath);
        
        /**
         * Frame-stable pointer. Never null for Placeholder types; null for FailFast on a stale or
         * invalid handle. Do not keep it across an Update().
         */
        template<CResource T>
        T* Resolve(ResourceHandle<T> InHandle) noexcept;
        
        /**
         * Turns a handle into a claim (adds a ref). Empty if the handle is stale: cannot reload.
         */
        template<CResource T>
        ResourceRef<T> Pin(ResourceHandle<T> InHandle);
        
        /**
         * Unloads every resource in every pool (warns about leaks). Call before the GPU context dies.
         */
        void FlushAll();

        /**
         * Sets the job system used by LoadAsync (set by the Engine at Startup). Defaults to inline loads.
         */
        void SetJobSystem(IJobSystem& InJobs) noexcept { m_Jobs = &InJobs; }
        
        /**
         * Counter incremented by each Update. Used to detect Resolve pointers kept across an Update.
         */
        Uint64 GetPumpEpoch() const noexcept { return m_PumpEpoch; }

    public:
        template<CResource T> Uint32 GetLoadedCount();
        template<CResource T> Uint32 GetLoadingCount();
        template<CResource T> Uint64 GetBytes();
        // Load state of a handle (Unloaded if stale or never loaded).
        template<CResource T> EResourceState GetState(ResourceHandle<T> InHandle);

    public:
        template<CResource T> void AddRef(ResourceHandle<T> InHandle) noexcept;
        template<CResource T> void Release(ResourceHandle<T> InHandle) noexcept;

        /**
         * Load used by dependency loads: shares the LoadContext, so cycles are detected across levels.
         */
        template<CResource T>
        ResourceRef<T> LoadInternal(const char* InPath, LoadContext& InCtx);
        
        /**
         * Records a parent -> child dependency, under the lock. May run on a worker.
         */
        void AddDependencyEdge(Uint32 InParentId, Uint32 InChildId);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        template<CResource T>
        ResourcePool<T>& GetOrCreatePool();

        // Calls the completion callbacks of settled loads (main thread, outside the lock)
        // and keeps the ones still loading.
        void FirePendingCallbacks();

        // =============================================================================
        // Override
        // =============================================================================
        
        //~Begin ISubsystem interface
    public:
        bool Startup() override;             // pools are created on demand
        void Shutdown() override;            // FlushAll + leak report
        void Update(double DeltaTime) override; // destroys released slots
        //~End ISubsystem interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        /**
         * One recursive lock for every pool, registry and graph change. Recursive because unloading
         * releases children on the same thread. Never held during T::Load.
         */
        mutable RecursiveMutex              m_Mutex;
        TDynArray<TUniquePtr<IResourcePool>> m_Pools; // indexed by ResourceTypeID::Get<T>()
        ResourceDependencyGraph             m_Deps;
        IJobSystem*                         m_Jobs = &IJobSystem::Null(); // LoadAsync worker pool
        Uint64                              m_PumpEpoch = 0; // incremented each Update

        // Pending LoadAsync callbacks. Each returns true once it has fired.
        TDynArray<TFunction<bool()>>        m_PendingCallbacks;
    };

    // =============================================================================
    // ResourceAsyncLoad — state shared between the worker (load) and the main thread (publish).
    // =============================================================================
    struct ResourceAsyncLoad
    {
        OpaaxString            Path;
        TUniquePtr<LoadContext> Ctx;
        bool                   Filled = false;
    };

    // =============================================================================
    // ResourceManager — template definitions
    // =============================================================================
    template<CResource T>
    ResourcePool<T>& ResourceManager::GetOrCreatePool()
    {
        const Uint32 lId = ResourceTypeID::Get<T>();
        if (lId >= m_Pools.size()) { m_Pools.resize(lId + 1); }
        if (!m_Pools[lId])         { m_Pools[lId] = MakeUnique<ResourcePool<T>>(); }
        // Safe: this slot only ever holds a ResourcePool<T>.
        return *static_cast<ResourcePool<T>*>(m_Pools[lId].get());
    }
    
    /**
     * Shared by Load and dependency loads: AcquireSlot (locked) -> FillSlot (unlocked, runs T::Load)
     * -> publish (locked; now for sync, in Update for async).
     */
    template<CResource T>
    ResourceRef<T> ResourceManager::LoadInternal(const char* InPath, LoadContext& InCtx)
    {
        const OpaaxStringID lId(InPath);
        if (!InCtx.PushLoading(lId.GetId())) // cycle check
        {
            OPAAX_LOG(LogResourceManager, Error, "Hard dependency cycle on '{}'", InPath);
            return ResourceRef<T>{ this, ResourceHandle<T>{} };
        }

        bool              lNeedsFill = false;
        ResourcePool<T>*  lPool      = nullptr;
        ResourceHandle<T> lHandle{};
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            lPool   = &GetOrCreatePool<T>();
            lHandle = lPool->AcquireSlot(InPath, lNeedsFill);
        }

        if (lNeedsFill)
        {
            const bool lOk = lPool->FillSlot(lHandle, InPath, InCtx); // T::Load, unlocked

            TLockGuard<RecursiveMutex> lLock(m_Mutex);
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
            // publish in Update
            else
            {
                lPool->FinalizeSlot(lHandle.Slot);
            }         // publish now (main thread)
        }

        InCtx.PopLoading();
        return ResourceRef<T>{ this, lHandle }; // takes the +1 from AcquireSlot
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::Load(const char* InPath)
    {
        LoadContext lCtx(*this, m_Deps); // sync: publishes inline
        return LoadInternal<T>(InPath, lCtx);
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::LoadAsync(const char* InPath, TFunction<void(LoadAsyncResult<T>)> InOnComplete)
    {
        ResourcePool<T>*  lPool      = nullptr;
        bool              lNeedsFill = false;
        ResourceHandle<T> lHandle{};
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            lPool   = &GetOrCreatePool<T>();
            lHandle = lPool->AcquireSlot(InPath, lNeedsFill); // Loading claim, returned now
        }

        if (lNeedsFill)
        {
            // Load the whole tree on one worker; publish in Update. Inline without a job system.
            TSharedPtr<ResourceAsyncLoad> lJob = MakeShared<ResourceAsyncLoad>();
            lJob->Path             = InPath; // owned across threads
            ResourceManager* lSelf = this;

            m_Jobs->Submit(
                [lSelf, lPool, lHandle, lJob]() // worker: no lock during T::Load
                {
                    lJob->Ctx    = MakeUnique<LoadContext>(*lSelf, lSelf->m_Deps, /*deferred*/ true);
                    lJob->Filled = lPool->FillSlot(lHandle, lJob->Path.CStr(), *lJob->Ctx);
                    if (lJob->Filled)
                    {
                        lJob->Ctx->AddPendingInit(lPool, lHandle.Slot);
                    } // root, after its children
                },
                [lSelf, lPool, lHandle, lJob]() // main thread: publish children first, or abandon
                {
                    TLockGuard<RecursiveMutex> lLock(lSelf->m_Mutex);
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

        ResourceRef<T> lRef{ this, lHandle }; // takes the +1 from AcquireSlot

        // Optional callback: an internal claim keeps the resource loaded until it fires.
        if (InOnComplete)
        {
            ResourceRef<T>   lClaim = lRef;
            ResourceManager* lSelf  = this;
            TFunction<bool()> lPoll =
                [lSelf, lClaim = Move(lClaim), lCb = Move(InOnComplete)]() -> bool
                {
                    const EResourceState lState = lSelf->GetState<T>(lClaim.GetHandle());
                    if (lState == EResourceState::Loading) { return false; } // still loading: poll again

                    LoadAsyncResult<T> lResult;
                    lResult.Status  = lState;
                    lResult.bFailed = (lState != EResourceState::Loaded);
                    if (!lResult.bFailed) { lResult.Ref = lClaim; }
                    lCb(Move(lResult));
                    return true; // fired: releases the internal claim
                };

            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            m_PendingCallbacks.emplace_back(Move(lPoll));
        }

        return lRef;
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::Find(const char* InPath)
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);

        const ResourceHandle<T> lHandle = GetOrCreatePool<T>().FindLoadedSlot(InPath);
        if (!lHandle.IsValid())
        {
            return ResourceRef<T>{};   // empty: Get() gives nullptr, not a placeholder
        }

        return ResourceRef<T>{ this, lHandle }; // takes the +1 from FindLoadedSlot
    }

    template<CResource T>
    bool ResourceManager::Reload(const char* InPath)
    {
        // Not loaded: nothing to reload (checked first to skip the parse).
        {
            TLockGuard<RecursiveMutex> lLock(m_Mutex);
            if (!GetOrCreatePool<T>().IsResident(InPath)) { return false; }
        }

        // Unlocked, like FillSlot: dependency loads go through this manager.
        LoadContext      lCtx(*this, m_Deps);
        std::optional<T> lLoaded = T::Load(InPath, lCtx);

        if (!lLoaded.has_value())
        {
            OPAAX_LOG(LogResourceManager, Warn, "Reload failed for '{}' — the resident copy is kept", InPath);
            return false;
        }

        TLockGuard<RecursiveMutex> lLock(m_Mutex);

        // Check again under the lock: it may have been released meanwhile (then drop the new data).
        const bool lSwapped = GetOrCreatePool<T>().ReplaceIfLoaded(InPath, Move(*lLoaded));

        if (lSwapped)
        {
            OPAAX_LOG(LogResourceManager, Info, "Reloaded '{}'", InPath);
        }

        return lSwapped;
    }

    template<CResource T>
    T* ResourceManager::Resolve(ResourceHandle<T> InHandle) noexcept
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().Get(InHandle);
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::Pin(ResourceHandle<T> InHandle)
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        ResourcePool<T>& lPool = GetOrCreatePool<T>();
        if (!lPool.IsLive(InHandle))
        {
            return ResourceRef<T>{};
        }
        
        lPool.AddRef(InHandle);
        return ResourceRef<T>{ this, InHandle };
    }

    template<CResource T>
    void ResourceManager::AddRef(ResourceHandle<T> InHandle) noexcept
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        GetOrCreatePool<T>().AddRef(InHandle);
    }

    template<CResource T>
    void ResourceManager::Release(ResourceHandle<T> InHandle) noexcept
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        GetOrCreatePool<T>().Release(InHandle);
    }

    template<CResource T>
    Uint32 ResourceManager::GetLoadedCount()
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetLoadedCount();
    }

    template<CResource T>
    Uint32 ResourceManager::GetLoadingCount()
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetLoadingCount();
    }

    template<CResource T>
    Uint64 ResourceManager::GetBytes()
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetBytes();
    }

    template<CResource T>
    EResourceState ResourceManager::GetState(ResourceHandle<T> InHandle)
    {
        TLockGuard<RecursiveMutex> lLock(m_Mutex);
        return GetOrCreatePool<T>().GetState(InHandle);
    }

    // =============================================================================
    // ResourceRef<T> — bodies that need the complete manager
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

    template<typename T>
    ResourceRef<T>::~ResourceRef()
    {
        if (m_Manager != nullptr && m_Handle.IsValid())
        {
            m_Manager->Release(m_Handle);
        }
    }

    template<typename T>
    T* ResourceRef<T>::Get() const noexcept
    {
        return (m_Manager != nullptr) ? m_Manager->Resolve(m_Handle) : nullptr;
    }

    // =============================================================================
    // LoadContext::Acquire — needs the complete manager
    // =============================================================================
    
    template<CResource TSub>
    ResourceRef<TSub> LoadContext::Acquire(const char* InPath)
    {
        const OpaaxStringID lChild(InPath);

        // Cycle: already loading higher in the chain. Fail, and record no dependency.
        if (IsLoading(lChild.GetId()))
        {
            OPAAX_LOG(LogResourceManager, Error, "Hard dependency cycle on '{}'", InPath);
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
