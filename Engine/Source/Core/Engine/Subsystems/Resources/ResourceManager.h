#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Core/Engine/Subsystems/EngineSubsystem.h"

#include "ResourceTypeID.hpp"
#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"
#include "ResourceDependencyGraph.hpp"
#include "ResourcePool.hpp"
#include "ResourceRef.hpp"
#include "LoadContext.hpp"

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
        ResourceManager() = default;
        // NOTE: FlushAll() in the body (see .cpp) — composite payloads hold child Refs
        // whose dtors call back into the manager, so everything must unload while the
        // manager + pools are still alive ("flush before context death", design §9).
        ~ResourceManager() override;

        // OPAAX_API force-instantiates the special members and m_Pools is move-only.
        ResourceManager(const ResourceManager&)            = delete;
        ResourceManager& operator=(const ResourceManager&) = delete;
        ResourceManager(ResourceManager&&)                 = delete;
        ResourceManager& operator=(ResourceManager&&)      = delete;

        // =============================================================================
        // Override — ISubsystem
        // =============================================================================
    public:
        bool Startup() override;             // builds nothing — pools are lazy
        void Shutdown() override;            // FlushAll + leak report
        void Update(double DeltaTime) override; // the pump (single mutation point)

        // =============================================================================
        // Frozen public API — Load / Resolve / Pin / FlushAll / Update
        // =============================================================================
    public:
        // Load (or dedup-share) a resource; returns an owning RAII claim.
        template<CResource T>
        ResourceRef<T> Load(const char* InPath);

        // Frame-stable view. O(1). Never null for Placeholder-policy types; null for
        // FailFast on stale/invalid. NEVER cache across a Resources.Update() pump.
        template<CResource T>
        T* Resolve(ResourceHandle<T> InHandle) noexcept;

        // Bridge a data Handle to a lifetime claim (adds a ref). Empty ref if the
        // handle is stale/dead — Pin cannot resurrect an unloaded resource.
        template<CResource T>
        ResourceRef<T> Pin(ResourceHandle<T> InHandle);

        // Unload every resource in every pool (warns on leaks). Flush BEFORE the GPU
        // context dies; destroy the manager after everything else (boot/shutdown order).
        void FlushAll();

        // =============================================================================
        // Diagnostics — bytes accounting from M1 (visibility first, budgets never)
        // =============================================================================
    public:
        template<CResource T> Uint32 GetLoadedCount();
        template<CResource T> Uint64 GetBytes();

        // =============================================================================
        // Internal plumbing — used by ResourceRef<T> and LoadContext (not game code)
        // =============================================================================
    public:
        template<CResource T> void AddRef(ResourceHandle<T> InHandle) noexcept;
        template<CResource T> void Release(ResourceHandle<T> InHandle) noexcept;

        // Recursion target for composite loads: shares the in-flight LoadContext so
        // the load chain (and cycle detection) accumulates across nested acquisitions.
        template<CResource T>
        ResourceRef<T> LoadInternal(const char* InPath, LoadContext& InCtx);

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        template<CResource T>
        ResourcePool<T>& GetOrCreatePool();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<UniquePtr<IResourcePool>> m_Pools; // indexed by ResourceTypeID::Get<T>()
        ResourceDependencyGraph             m_Deps;
    };

    // =============================================================================
    // ResourceManager — template definitions (all types complete below this line)
    // =============================================================================
    template<CResource T>
    ResourcePool<T>& ResourceManager::GetOrCreatePool()
    {
        const Uint32 lId = ResourceTypeID::Get<T>();
        if (lId >= m_Pools.size()) { m_Pools.resize(lId + 1); }
        if (!m_Pools[lId])         { m_Pools[lId] = MakeUnique<ResourcePool<T>>(); }
        // NOTE: safe — the slot at lId is only ever populated with a ResourcePool<T>.
        return *static_cast<ResourcePool<T>*>(m_Pools[lId].get());
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::Load(const char* InPath)
    {
        LoadContext lCtx(*this, m_Deps);
        return LoadInternal<T>(InPath, lCtx);
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::LoadInternal(const char* InPath, LoadContext& InCtx)
    {
        const OpaaxStringID lId(InPath);
        if (!InCtx.PushLoading(lId.GetId()))
        {
            // A path already in the in-flight chain — a hard dependency cycle.
            OPAAX_CORE_ERROR("[Resources] Hard dependency cycle on '{}'", InPath);
            return ResourceRef<T>{ this, ResourceHandle<T>{} };
        }

        const ResourceHandle<T> lHandle = GetOrCreatePool<T>().Load(InPath, InCtx);
        InCtx.PopLoading();

        // Adopt the +1 the pool applied (invalid handle -> Get yields placeholder/null).
        return ResourceRef<T>{ this, lHandle };
    }

    template<CResource T>
    T* ResourceManager::Resolve(ResourceHandle<T> InHandle) noexcept
    {
        return GetOrCreatePool<T>().Get(InHandle);
    }

    template<CResource T>
    ResourceRef<T> ResourceManager::Pin(ResourceHandle<T> InHandle)
    {
        ResourcePool<T>& lPool = GetOrCreatePool<T>();
        if (!lPool.IsLive(InHandle)) { return ResourceRef<T>{}; }
        lPool.AddRef(InHandle);
        return ResourceRef<T>{ this, InHandle };
    }

    template<CResource T>
    void ResourceManager::AddRef(ResourceHandle<T> InHandle) noexcept
    {
        GetOrCreatePool<T>().AddRef(InHandle);
    }

    template<CResource T>
    void ResourceManager::Release(ResourceHandle<T> InHandle) noexcept
    {
        GetOrCreatePool<T>().Release(InHandle);
    }

    template<CResource T>
    Uint32 ResourceManager::GetLoadedCount()
    {
        return GetOrCreatePool<T>().GetLoadedCount();
    }

    template<CResource T>
    Uint64 ResourceManager::GetBytes()
    {
        return GetOrCreatePool<T>().GetBytes();
    }

    // =============================================================================
    // ResourceRef<T> — crossing bodies (manager complete)
    // =============================================================================
    template<typename T>
    ResourceRef<T>::ResourceRef(const ResourceRef& InOther)
        : m_Manager(InOther.m_Manager)
        , m_Handle(InOther.m_Handle)
    {
        if (m_Manager != nullptr && m_Handle.IsValid()) { m_Manager->AddRef(m_Handle); }
    }

    template<typename T>
    ResourceRef<T>& ResourceRef<T>::operator=(const ResourceRef& InOther)
    {
        if (this == &InOther) { return *this; }
        if (m_Manager != nullptr && m_Handle.IsValid()) { m_Manager->Release(m_Handle); }
        m_Manager = InOther.m_Manager;
        m_Handle  = InOther.m_Handle;
        if (m_Manager != nullptr && m_Handle.IsValid()) { m_Manager->AddRef(m_Handle); }
        return *this;
    }

    template<typename T>
    ResourceRef<T>& ResourceRef<T>::operator=(ResourceRef&& InOther) noexcept
    {
        if (this == &InOther) { return *this; }
        if (m_Manager != nullptr && m_Handle.IsValid()) { m_Manager->Release(m_Handle); }
        m_Manager = InOther.m_Manager;
        m_Handle  = InOther.m_Handle;
        InOther.m_Manager = nullptr;
        InOther.m_Handle  = ResourceHandle<T>{};
        return *this;
    }

    template<typename T>
    ResourceRef<T>::~ResourceRef()
    {
        if (m_Manager != nullptr && m_Handle.IsValid()) { m_Manager->Release(m_Handle); }
    }

    template<typename T>
    T* ResourceRef<T>::Get() const noexcept
    {
        return (m_Manager != nullptr) ? m_Manager->Resolve(m_Handle) : nullptr;
    }

    // =============================================================================
    // LoadContext::Acquire — crossing body (manager complete)
    // =============================================================================
    template<CResource TSub>
    ResourceRef<TSub> LoadContext::Acquire(const char* InPath)
    {
        const OpaaxStringID lChild(InPath);

        // Cycle: acquiring something already loading up the chain. Fail loudly, do NOT
        // record an edge (keeps the hard-reference graph a DAG).
        if (IsLoading(lChild.GetId()))
        {
            OPAAX_CORE_ERROR("[Resources] Hard dependency cycle on '{}'", InPath);
            return ResourceRef<TSub>{ &m_Manager, ResourceHandle<TSub>{} };
        }

        const Uint32 lParent = CurrentParent();
        if (lParent != OpaaxGlobal::ID_None)
        {
            m_Deps.AddEdge(lParent, lChild.GetId());
        }

        return m_Manager.LoadInternal<TSub>(InPath, *this);
    }
}
