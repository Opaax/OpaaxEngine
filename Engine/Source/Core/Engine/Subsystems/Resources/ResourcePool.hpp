#pragma once

#include <concepts>
#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Log/OpaaxLog.h"

#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"

// =============================================================================
// ResourcePool<T> — storage. One pool per resource type, lazy-created by the
// manager. Owns the payloads, the metadata, the free list and the path-dedup
// table. IResourcePool is the ONE virtual in the whole system — type-erased pool
// ownership + shutdown for the manager, never per-resource, never hot.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourcePool{"ResourcePool"};
    
    // =============================================================================
    // IResourcePool — manager plumbing (type-erased ownership + shutdown).
    // =============================================================================
    class OPAAX_API IResourcePool
    {
    public:
        virtual ~IResourcePool()                        = default;
        virtual void   UnloadAll() noexcept             = 0;
        virtual Uint32 GetLoadedCount() const noexcept  = 0;
        virtual Uint64 GetBytes() const noexcept        = 0;
    };

    // =============================================================================
    // ResourcePool<T> — generational, chunked slot pool.
    // =============================================================================
    template<CResource T>
    class ResourcePool final : public IResourcePool
    {
        // -------------------------------------------------------------------------
        // Chunked payload storage: fixed-size chunks, heap-owned. Growth allocates a
        // NEW chunk — existing payload addresses never move. Slots are reused in
        // place; a payload's address dies only when it unloads/reloads. This is what
        // makes Resolve()'s pointer frame-stable, incl. synchronous mid-frame loads.
        // -------------------------------------------------------------------------
        static constexpr Uint32 ChunkSize = 64;
        using Chunk = TFixedArray<std::optional<T>, ChunkSize>;

        struct SlotMeta
        {
            Uint32         Generation = 1;               // bumped on unload -> stale handles resolve safe
            Uint32         RefCount   = 0;
            EResourceState State      = EResourceState::Unloaded;
            OpaaxStringID  Source;                        // dedup key + GetSourcePath (M-RES-ED)
            Uint64         Bytes      = 0;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        using HandleType = ResourceHandle<T>;

        // ------ Load: dedup, then cold-load via the type's static Load ------------
        HandleType Load(const char* InPath, LoadContext& InCtx)
        {
            const OpaaxStringID lId(InPath);
            const Uint32        lKey = lId.GetId();

            // Dedup: one copy per unique path. A live slot ⇒ share it, refcount++.
            if (const auto lIt = m_PathToSlot.find(lKey); lIt != m_PathToSlot.end())
            {
                const Uint32 lSlot = lIt->second;
                SlotMeta&    lMeta = m_Meta[lSlot];
                if (lMeta.State == EResourceState::Loaded)
                {
                    ++lMeta.RefCount;
                    return HandleType{ lSlot, lMeta.Generation };
                }
            }

            // Cold load — Load returns a fully-formed object or nothing (no exceptions).
            std::optional<T> lLoaded = T::Load(InPath, InCtx);
            if (!lLoaded.has_value())
            {
                OPAAX_LOG(LogResourcePool, Error, "Load failed: '{}'", InPath)
                return HandleType{}; // invalid — Resolve yields placeholder/null per policy
            }

            const Uint32 lSlot = AllocSlot();
            PayloadRef(lSlot).emplace(Move(*lLoaded));

            SlotMeta& lMeta   = m_Meta[lSlot];
            lMeta.State       = EResourceState::Loaded;
            lMeta.RefCount    = 1;
            lMeta.Source      = lId;
            lMeta.Bytes       = ComputeBytes(PayloadRef(lSlot).value());

            m_PathToSlot[lKey] = lSlot;
            ++m_LoadedCount;
            m_TotalBytes += lMeta.Bytes;

            return HandleType{ lSlot, lMeta.Generation };
        }

        // ------ Resolve: O(1) frame-stable view. Never null for Placeholder types --
        T* Get(HandleType InHandle) noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_Meta.size())
            {
                return PlaceholderOrNull();
            }
            SlotMeta& lMeta = m_Meta[InHandle.Slot];
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation)
            {
                return PlaceholderOrNull();
            }
            return &PayloadRef(InHandle.Slot).value();
        }

        // True if the handle points at the current, loaded occupant of its slot.
        bool IsLive(HandleType InHandle) noexcept { return LiveMeta(InHandle) != nullptr; }

        void AddRef(HandleType InHandle) noexcept
        {
            if (SlotMeta* lMeta = LiveMeta(InHandle)) { ++lMeta->RefCount; }
        }

        void Release(HandleType InHandle) noexcept
        {
            SlotMeta* lMeta = LiveMeta(InHandle);
            if (lMeta == nullptr) { return; }
            if (--lMeta->RefCount == 0)
            {
                Unload(InHandle.Slot);
            }
        }

        const OpaaxStringID* GetSource(HandleType InHandle) const noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_Meta.size()) { return nullptr; }
            const SlotMeta& lMeta = m_Meta[InHandle.Slot];
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation) { return nullptr; }
            return &lMeta.Source;
        }

        // =============================================================================
        // Override — IResourcePool
        // =============================================================================
    public:
        void UnloadAll() noexcept override
        {
            for (Uint32 lSlot = 0; lSlot < m_Meta.size(); ++lSlot)
            {
                SlotMeta& lMeta = m_Meta[lSlot];
                if (lMeta.State != EResourceState::Loaded) { continue; }
                if (lMeta.RefCount > 0)
                {
                    OPAAX_CORE_WARN("[Resources] Leak at flush: '{}' (refcount {})", lMeta.Source, lMeta.RefCount);
                }
                PayloadRef(lSlot).reset(); // may cascade-release composite children on OTHER pools
                ++lMeta.Generation;
                lMeta.State    = EResourceState::Unloaded;
                lMeta.RefCount = 0;
                lMeta.Bytes    = 0;
            }
            m_PathToSlot.clear();
            m_LoadedCount = 0;
            m_TotalBytes  = 0;
        }

        Uint32 GetLoadedCount() const noexcept override { return m_LoadedCount; }
        Uint64 GetBytes()       const noexcept override { return m_TotalBytes;  }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        std::optional<T>& PayloadRef(Uint32 InSlot) noexcept
        {
            return (*m_Chunks[InSlot / ChunkSize])[InSlot % ChunkSize];
        }

        // Resolve a handle to its slot meta iff it is live (valid, in range, current
        // generation, Loaded) — the shared guard for AddRef/Release.
        SlotMeta* LiveMeta(HandleType InHandle) noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_Meta.size())
            {
                return nullptr;
            }
            
            SlotMeta& lMeta = m_Meta[InHandle.Slot];
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation)
            {
                return nullptr;
            }
            
            return &lMeta;
        }

        Uint32 AllocSlot()
        {
            if (!m_FreeSlots.empty())
            {
                const Uint32 lSlot = m_FreeSlots.back();
                m_FreeSlots.pop_back();
                return lSlot; // keeps its bumped Generation from the prior unload
            }
            const Uint32 lSlot = static_cast<Uint32>(m_Meta.size());
            m_Meta.push_back(SlotMeta{});
            if (lSlot / ChunkSize >= m_Chunks.size())
            {
                m_Chunks.push_back(MakeUnique<Chunk>());
            }
            return lSlot;
        }

        void Unload(Uint32 InSlot) noexcept
        {
            SlotMeta&    lMeta      = m_Meta[InSlot];
            const Uint32 lSourceKey = lMeta.Source.GetId();

            // Bookkeeping BEFORE destroying the payload: bump generation first so a
            // re-entrant Get (composite child cascade) already sees this slot as gone.
            m_TotalBytes -= lMeta.Bytes;
            --m_LoadedCount;
            m_PathToSlot.erase(lSourceKey);

            ++lMeta.Generation;
            lMeta.State    = EResourceState::Unloaded;
            lMeta.RefCount = 0;
            lMeta.Bytes    = 0;
            lMeta.Source   = OpaaxStringID{};
            m_FreeSlots.push_back(InSlot);

            // Destroy LAST — a composite payload holds child Refs whose dtors call
            // Release on their pools (M-RES-1: immediate; M-RES-2 defers this to the pump).
            PayloadRef(InSlot).reset();
        }

        T* PlaceholderOrNull() noexcept
        {
            if constexpr (T::FailPolicy == EFailPolicy::Placeholder)
            {
                if (!m_Placeholder.has_value()) { m_Placeholder.emplace(T::Placeholder()); }
                return &m_Placeholder.value();
            }
            else
            {
                return nullptr; // FailFast: stale/invalid resolves to null, never lies
            }
        }

        static Uint64 ComputeBytes(const T& InValue) noexcept
        {
            // NOTE: honor an optional ByteSize() for real payload accounting (file/GPU
            // bytes); fall back to the struct size when the type doesn't provide it.
            if constexpr (requires(const T& v) { { v.ByteSize() } -> std::convertible_to<Uint64>; })
            {
                return InValue.ByteSize();
            }
            else
            {
                return sizeof(T);
            }
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<UniquePtr<Chunk>>  m_Chunks;      // address-stable payload storage
        TDynArray<SlotMeta>          m_Meta;        // parallel to slots (may relocate; payloads may not)
        TDynArray<Uint32>            m_FreeSlots;   // reusable slot indices
        UnorderedMap<Uint32, Uint32> m_PathToSlot;  // interned path id -> slot (dedup)
        std::optional<T>             m_Placeholder; // built once, on first fallback
        Uint32                       m_LoadedCount = 0;
        Uint64                       m_TotalBytes  = 0;
    };
}
