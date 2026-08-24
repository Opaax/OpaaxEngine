#pragma once

#include <concepts>
#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

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
        virtual void   CollectGarbage() noexcept        = 0; // destroy the previous pump's released slots
        virtual void   FinalizeSlot(Uint32 InSlot) noexcept = 0; // main-thread publish of a filled async slot
        virtual Uint32 GetLoadedCount() const noexcept  = 0;
        virtual Uint32 GetLoadingCount() const noexcept = 0;
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

        /**
         * Generation : bumped on unload -> stale handles resolve safe
         * dedup key + GetSourcePath (M-RES-ED)
         */
        struct SlotMeta
        {
            Uint32         Generation = 1;
            Uint32         RefCount   = 0;
            EResourceState State      = EResourceState::Unloaded;
            OpaaxStringID  Source;
            Uint64         Bytes      = 0;
        };

        // Metadata is chunked alongside the payloads (same ChunkSize) so a SlotMeta&
        // stays address-stable across growth — a worker may append slots while the
        // main thread reads meta in Resolve. The outer pointer vectors are reserved
        // to MaxChunks so those appends never relocate them (keeps Resolve lock-free).
        using MetaChunk = TFixedArray<SlotMeta, ChunkSize>;
        static constexpr Uint32 MaxChunks = 1024; // 65536 slots / resource type

        // A slot whose refcount hit 0 — destroyed at the NEXT CollectGarbage() pump,
        // not immediately. Generation pins the occupant so a resurrected/reused slot
        // is recognised and skipped when the grave entry is finally processed.
        struct GraveEntry
        {
            Uint32 Slot;
            Uint32 Generation;
        };

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        using HandleType = ResourceHandle<T>;

        ResourcePool()
        {
            // Pre-reserve so worker slot allocation only appends (never relocates)
            // the outer vectors that Resolve reads lock-free.
            m_Chunks.reserve(MaxChunks);
            m_MetaChunks.reserve(MaxChunks);
        }

        // ---------------------------------------------------------------------
        // Load split (dedup -> alloc Loading | fill | publish) — orchestrated by the
        // manager under its lock. Splitting AcquireSlot from FillSlot is what lets an
        // async root return its handle immediately, then fill on a worker, then publish
        // on the pump. The synchronous path runs all three back-to-back.
        // ---------------------------------------------------------------------

        // Dedup or allocate a Loading slot. bOutNeedsFill=false => shared an existing
        // Loaded/Loading slot (refcount bumped); true => a fresh Loading slot to Fill.
        HandleType AcquireSlot(const char* InPath, bool& bOutNeedsFill)
        {
            const OpaaxStringID lId(InPath);
            const Uint32        lKey = lId.GetId();

            // Share one copy per unique path — a Loaded OR still-Loading slot, so two
            // requests for the same in-flight resource collapse to a single load.
            if (const auto lIt = m_PathToSlot.find(lKey); lIt != m_PathToSlot.end())
            {
                SlotMeta& lMeta = MetaRef(lIt->second);
                if (lMeta.State == EResourceState::Loaded || lMeta.State == EResourceState::Loading)
                {
                    ++lMeta.RefCount;
                    bOutNeedsFill = false;
                    return HandleType{ lIt->second, lMeta.Generation };
                }
            }

            const Uint32 lSlot = AllocSlot();
            SlotMeta&    lMeta = MetaRef(lSlot);
            lMeta.State    = EResourceState::Loading;
            lMeta.RefCount = 1;
            lMeta.Source   = lId;
            lMeta.Bytes    = 0;
            m_PathToSlot[lKey] = lSlot;
            ++m_LoadingCount;
            bOutNeedsFill = true;
            return HandleType{ lSlot, lMeta.Generation };
        }

        // Dedup WITHOUT allocating: a claim on a path that is ALREADY Loaded, or an invalid handle.
        // AcquireSlot's first half with the second half refused — the editor's question ("is this
        // already resident?") must never turn into a load, or browsing a folder would pull every
        // file in it into memory.
        //
        // Loading counts as NOT found on purpose: the caller wants something it can display now,
        // and an in-flight slot has no payload yet.
        HandleType FindLoadedSlot(const char* InPath)
        {
            const OpaaxStringID lId(InPath);

            const auto lIt = m_PathToSlot.find(lId.GetId());
            if (lIt == m_PathToSlot.end())
            {
                return HandleType{};
            }

            SlotMeta& lMeta = MetaRef(lIt->second);
            if (lMeta.State != EResourceState::Loaded)
            {
                return HandleType{};
            }

            ++lMeta.RefCount;   // the caller adopts it, as it does an AcquireSlot dedup hit
            return HandleType{ lIt->second, lMeta.Generation };
        }

        // Produce the payload via the type's Load. The caller runs this OUTSIDE the lock
        // (a composite's child Acquires recurse through the manager, which re-locks). The
        // slot is exclusively owned while Loading, so the emplace/Bytes write need no lock.
        bool FillSlot(HandleType InHandle, const char* InPath, LoadContext& InCtx)
        {
            std::optional<T> lLoaded = T::Load(InPath, InCtx);
            if (!lLoaded.has_value())
            {
                OPAAX_LOG(LogResourcePool, Error, "Load failed: '{}'", InPath);
                return false; // slot left Loading for the caller to Abandon
            }
            PayloadRef(InHandle.Slot).emplace(Move(*lLoaded));
            MetaRef(InHandle.Slot).Bytes = ComputeBytes(PayloadRef(InHandle.Slot).value());
            return true;
        }

        // Main-thread publish of a filled slot: Initialize (GPU log-in) + flip Loaded if
        // still referenced, else abandon it (a parent load failed and released it before
        // this pump). Virtual — LoadContext::PublishAll drives it type-erased.
        void FinalizeSlot(Uint32 InSlot) noexcept override
        {
            if (InSlot >= m_SlotCount) { return; }
            SlotMeta& lMeta = MetaRef(InSlot);
            if (lMeta.State != EResourceState::Loading)
            {
                return;
            } // already handled

            if (lMeta.RefCount == 0)
            {
                AbandonSlot(InSlot); // orphaned before publish
                return;
            }
            MaybeInitialize(PayloadRef(InSlot).value()); // GPU log-in, main thread
            lMeta.State = EResourceState::Loaded;
            --m_LoadingCount;
            ++m_LoadedCount;
            m_TotalBytes += lMeta.Bytes;
        }

        // Discard a Loading slot (fill failed, or orphaned). Frees it + bumps generation
        // so any handle to it resolves stale-safe.
        void AbandonSlot(Uint32 InSlot) noexcept
        {
            SlotMeta& lMeta = MetaRef(InSlot);
            if (lMeta.State != EResourceState::Loading) { return; }
            m_PathToSlot.erase(lMeta.Source.GetId());
            ++lMeta.Generation;
            lMeta.State    = EResourceState::Unloaded;
            lMeta.RefCount = 0;
            lMeta.Bytes    = 0;
            lMeta.Source   = OpaaxStringID{};
            --m_LoadingCount;
            m_FreeSlots.emplace_back(InSlot);
            PayloadRef(InSlot).reset(); // orphan composite -> child releases (manager re-locks)
        }


        /**
         * O(1) frame-stable view. Never null for Placeholder types
         * @param InHandle 
         * @return 
         */
        T* Get(HandleType InHandle) noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_SlotCount)
            {
                return PlaceholderOrNull();
            }
            SlotMeta& lMeta = MetaRef(InHandle.Slot);
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation)
            {
                return PlaceholderOrNull();
            }
            return &PayloadRef(InHandle.Slot).value();
        }
        
        /**
         * 
         * @param InHandle 
         * @return True if the handle points at the current, loaded occupant of its slot.
         */
        bool IsLive(HandleType InHandle) noexcept { return LiveMeta(InHandle) != nullptr; }

        void AddRef(HandleType InHandle) noexcept
        {
            if (SlotMeta* lMeta = RefMeta(InHandle)) { ++lMeta->RefCount; }
        }

        /***/
        void Release(HandleType InHandle) noexcept
        {
            SlotMeta* lMeta = RefMeta(InHandle);
            if (lMeta == nullptr) { return; }
            if (--lMeta->RefCount == 0 && lMeta->State == EResourceState::Loaded)
            {
                // Deferred unload (Loaded only): the payload stays valid until the next
                // CollectGarbage() pump, so a Resolve()'d pointer survives the rest of the
                // frame and a dedup Load before the pump resurrects it. A LOADING slot
                // dropped to 0 has no payload to defer — FinalizeSlot abandons it at publish.
                m_Graveyard.emplace_back(InHandle.Slot, lMeta->Generation);
            }
        }

        /***/
        const OpaaxStringID* GetSource(HandleType InHandle) const noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_SlotCount) { return nullptr; }
            const SlotMeta& lMeta = MetaRef(InHandle.Slot);
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation) { return nullptr; }
            return &lMeta.Source;
        }

        // Current state of a handle's slot; Unloaded if out of range or generation-stale.
        EResourceState GetState(HandleType InHandle) const noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_SlotCount) { return EResourceState::Unloaded; }
            const SlotMeta& lMeta = MetaRef(InHandle.Slot);
            if (lMeta.Generation != InHandle.Generation) { return EResourceState::Unloaded; }
            return lMeta.State;
        }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IResourcePool interface
    public:
        /***/
        void UnloadAll() noexcept override
        {
            for (Uint32 lSlot = 0; lSlot < m_SlotCount; ++lSlot)
            {
                SlotMeta& lMeta = MetaRef(lSlot);
                if (lMeta.State != EResourceState::Loaded) { continue; }
                if (lMeta.RefCount > 0)
                {
                    OPAAX_LOG(LogResourcePool, Warn, "Leak at flush: '{}' (refcount {})", lMeta.Source, lMeta.RefCount);
                }
                PayloadRef(lSlot).reset(); // may cascade-release composite children on OTHER pools
                ++lMeta.Generation;
                lMeta.State    = EResourceState::Unloaded;
                lMeta.RefCount = 0;
                lMeta.Bytes    = 0;
            }
            m_PathToSlot.clear();
            m_Graveyard.clear();
            m_LoadedCount  = 0;
            m_LoadingCount = 0;
            m_TotalBytes   = 0;
        }

        /**
         * Destroy every slot released before this pump. Processes a SNAPSHOT: unloading
         * a composite cascade-releases its children, which enqueue fresh grave entries
         * for the NEXT pump — bounding per-pump work and giving those children the same
         * one-frame grace. A resurrected/reused slot (refcount>0 or generation moved) is
         * skipped, so the entry harmlessly no-ops.
         */
        void CollectGarbage() noexcept override
        {
            if (m_Graveyard.empty()) { return; }

            TDynArray<GraveEntry> lPending;
            lPending.swap(m_Graveyard);
            for (const GraveEntry& lEntry : lPending)
            {
                if (lEntry.Slot >= m_SlotCount) { continue; }
                SlotMeta& lMeta = MetaRef(lEntry.Slot);
                if (lMeta.State != EResourceState::Loaded)   { continue; } // already gone
                if (lMeta.Generation != lEntry.Generation)   { continue; } // reused since
                if (lMeta.RefCount != 0)                     { continue; } // resurrected
                Unload(lEntry.Slot);
            }
        }
        /***/
        Uint32 GetLoadedCount()  const noexcept override { return m_LoadedCount;  }
        /***/
        Uint32 GetLoadingCount() const noexcept override { return m_LoadingCount; }
        /***/
        Uint64 GetBytes()        const noexcept override { return m_TotalBytes;   }
        //~End IResourcePool interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /***/
        std::optional<T>& PayloadRef(Uint32 InSlot) noexcept
        {
            return (*m_Chunks[InSlot / ChunkSize])[InSlot % ChunkSize];
        }

        // Address-stable metadata access — the heap chunk never moves, so a returned
        // SlotMeta& survives concurrent slot allocation on another thread.
        SlotMeta&       MetaRef(Uint32 InSlot)       noexcept { return (*m_MetaChunks[InSlot / ChunkSize])[InSlot % ChunkSize]; }
        const SlotMeta& MetaRef(Uint32 InSlot) const noexcept { return (*m_MetaChunks[InSlot / ChunkSize])[InSlot % ChunkSize]; }

        /**
         * Resolve a handle to its slot meta iff it is live (valid, in range, current generation, Loaded) — the shared guard for AddRef/Release.
         * @param InHandle 
         * @return 
         */
        SlotMeta* LiveMeta(HandleType InHandle) noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_SlotCount)
            {
                return nullptr;
            }

            SlotMeta& lMeta = MetaRef(InHandle.Slot);
            if (lMeta.State != EResourceState::Loaded || lMeta.Generation != InHandle.Generation)
            {
                return nullptr;
            }

            return &lMeta;
        }

        // Meta for REFCOUNTING — accepts Loaded OR Loading (unlike LiveMeta, which is
        // Loaded-only for Resolve). A ResourceRef to an async load can be copied/dropped
        // BEFORE it publishes, so AddRef/Release must track in-flight (Loading) slots.
        SlotMeta* RefMeta(HandleType InHandle) noexcept
        {
            if (!InHandle.IsValid() || InHandle.Slot >= m_SlotCount)
            {
                return nullptr;
            }

            SlotMeta& lMeta = MetaRef(InHandle.Slot);
            if (lMeta.Generation != InHandle.Generation)
            {
                return nullptr;
            }
            if (lMeta.State != EResourceState::Loaded && lMeta.State != EResourceState::Loading)
            {
                return nullptr;
            }

            return &lMeta;
        }
        /***/
        Uint32 AllocSlot()
        {
            if (!m_FreeSlots.empty())
            {
                const Uint32 lSlot = m_FreeSlots.back();
                m_FreeSlots.pop_back();
                return lSlot; // keeps its bumped Generation from the prior unload
            }
            const Uint32 lSlot = m_SlotCount++;
            if (lSlot / ChunkSize >= m_Chunks.size())
            {
                // New chunk: payload + meta grow together. A fresh MetaChunk default-
                // constructs 64 SlotMeta{} (Generation 1, Unloaded). Appends must stay
                // within the reserved MaxChunks or the outer vectors relocate and break
                // lock-free Resolve.
                OPAAX_ASSERT(m_Chunks.size() < MaxChunks) // exceeded MaxChunks -> Resolve no longer lock-free
                m_Chunks.emplace_back(MakeUnique<Chunk>());
                m_MetaChunks.emplace_back(MakeUnique<MetaChunk>());
            }
            return lSlot;
        }
        /***/
        void Unload(Uint32 InSlot) noexcept
        {
            SlotMeta&    lMeta      = MetaRef(InSlot);
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
            m_FreeSlots.emplace_back(InSlot);

            // Destroy LAST — a composite payload holds child Refs whose dtors Release on
            // their pools, enqueuing those children into their own next-pump graveyard.
            PayloadRef(InSlot).reset();
        }
        /***/
        T* PlaceholderOrNull() noexcept
        {
            if constexpr (T::FailPolicy == EFailPolicy::Placeholder)
            {
                if (!m_Placeholder.has_value())
                {
                    // A placeholder is a FULLY-INITIALISED resource, not a half-built one: it goes
                    // through the same main-thread log-in a loaded payload does, or a GPU-backed
                    // type's substitute would have no texture and fail silently (as a black quad).
                    // Built on first fallback, from Resolve — i.e. on the main thread, like the pump.
                    m_Placeholder.emplace(T::Placeholder());
                    MaybeInitialize(m_Placeholder.value());
                }

                return &m_Placeholder.value();
            }
            else
            {
                return nullptr; // FailFast: stale/invalid resolves to null, never lies
            }
        }
        /***/
        static void MaybeInitialize(T& InValue) noexcept
        {
            // Main-thread "log-in" (GPU upload, handle registration) if the type
            // provides Initialize(); leaf/CPU types without it are a compile-time no-op.
            if constexpr (requires(T& v) { v.Initialize(); })
            {
                InValue.Initialize();
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
        TDynArray<TUniquePtr<Chunk>>     m_Chunks;      // address-stable payload storage
        TDynArray<TUniquePtr<MetaChunk>> m_MetaChunks;  // address-stable slot metadata (parallel to payload chunks)
        Uint32                          m_SlotCount = 0; // high-water slot index (monotonic); range-checks Resolve lock-free
        TDynArray<Uint32>               m_FreeSlots;   // reusable slot indices
        TDynArray<GraveEntry>           m_Graveyard;   // refcount-0 slots awaiting the next pump
        TUnorderedMap<Uint32, Uint32>    m_PathToSlot;  // interned path id -> slot (dedup)
        std::optional<T>                m_Placeholder; // built once, on first fallback
        Uint32                          m_LoadedCount  = 0;
        Uint32                          m_LoadingCount = 0; // in-flight async slots (wired in the async step)
        Uint64                          m_TotalBytes   = 0;
    };
}
