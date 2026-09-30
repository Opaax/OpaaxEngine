#pragma once

#include <concepts>
#include <optional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"

#include "ResourceConcept.hpp"
#include "ResourceHandle.hpp"

// =============================================================================
// ResourcePool<T> — storage for one resource type (created on demand by the manager).
// Owns the payloads, the metadata, the free list and the path lookup.
// =============================================================================
namespace Opaax
{
    inline constexpr LogCategory LogResourcePool{"ResourcePool"};
    
    // =============================================================================
    // IResourcePool — type-erased interface for the manager.
    // =============================================================================
    class OPAAX_API IResourcePool
    {
    public:
        virtual ~IResourcePool()                        = default;
        virtual void   UnloadAll() noexcept             = 0;
        virtual void   CollectGarbage() noexcept        = 0; // destroy the slots released last Update
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
        // Chunked storage: growing adds a chunk, so payload addresses never move.
        // This keeps Resolve()'s pointer valid for the frame.
        // -------------------------------------------------------------------------
        static constexpr Uint32 ChunkSize = 64;
        using Chunk = TFixedArray<std::optional<T>, ChunkSize>;

        /**
         * Generation: bumped on unload, so stale handles resolve safely.
         */
        struct SlotMeta
        {
            Uint32         Generation = 1;
            Uint32         RefCount   = 0;
            EResourceState State      = EResourceState::Unloaded;
            OpaaxStringID  Source;
            Uint64         Bytes      = 0;
        };

        // Metadata is chunked like the payloads, so a SlotMeta& stays valid while a worker
        // adds slots. The outer vectors are reserved to MaxChunks so they never move (lock-free Resolve).
        using MetaChunk = TFixedArray<SlotMeta, ChunkSize>;
        static constexpr Uint32 MaxChunks = 1024; // 65536 slots per type

        // A slot whose refcount reached 0 — destroyed at the next CollectGarbage(), not immediately.
        // The generation detects a slot that was reused meanwhile.
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
            // Reserve so adding slots never moves the outer vectors (read lock-free by Resolve).
            m_Chunks.reserve(MaxChunks);
            m_MetaChunks.reserve(MaxChunks);
        }

        // ---------------------------------------------------------------------
        // Load in three steps (find or allocate | fill | publish), driven by the manager.
        // Async loads return the handle after the first step, fill on a worker, publish in Update.
        // ---------------------------------------------------------------------

        // Share or allocate a Loading slot. bOutNeedsFill is false when an existing slot was shared.
        HandleType AcquireSlot(const char* InPath, bool& bOutNeedsFill)
        {
            const OpaaxStringID lId(InPath);
            const Uint32        lKey = lId.GetId();

            // One slot per path: two requests for the same file share a single load.
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

        // Like AcquireSlot but never allocates: a claim on an already Loaded path, or an invalid handle.
        // A Loading slot counts as not found (no payload yet).
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

            ++lMeta.RefCount;   // the caller takes the ref
            return HandleType{ lIt->second, lMeta.Generation };
        }

        // Runs T::Load. Called outside the lock (dependency loads go through the manager).
        bool FillSlot(HandleType InHandle, const char* InPath, LoadContext& InCtx)
        {
            std::optional<T> lLoaded = T::Load(InPath, InCtx);
            if (!lLoaded.has_value())
            {
                OPAAX_LOG(LogResourcePool, Error, "Load failed: '{}'", InPath);
                return false; // left Loading, the caller abandons it
            }
            PayloadRef(InHandle.Slot).emplace(Move(*lLoaded));
            MetaRef(InHandle.Slot).Bytes = ComputeBytes(PayloadRef(InHandle.Slot).value());
            return true;
        }

        /** Whether InPath has a Loaded slot. */
        bool IsResident(const char* InPath) const
        {
            const OpaaxStringID lId(InPath);

            const auto lIt = m_PathToSlot.find(lId.GetId());

            return lIt != m_PathToSlot.end() && MetaRef(lIt->second).State == EResourceState::Loaded;
        }

        /**
         * Replaces the payload of a loaded path (hot reload). Handle, refcount and generation are kept,
         * so every ref sees the new data. Main thread only.
         * @return False if InPath is not loaded, or still loading
         */
        bool ReplaceIfLoaded(const char* InPath, T&& InValue)
        {
            const OpaaxStringID lId(InPath);

            const auto lIt = m_PathToSlot.find(lId.GetId());
            if (lIt == m_PathToSlot.end()) { return false; }

            SlotMeta& lMeta = MetaRef(lIt->second);
            if (lMeta.State != EResourceState::Loaded) { return false; }

            MaybeInitialize(InValue);

            m_TotalBytes -= lMeta.Bytes;
            PayloadRef(lIt->second).emplace(Move(InValue));   // the old payload is destroyed here
            lMeta.Bytes   = ComputeBytes(PayloadRef(lIt->second).value());
            m_TotalBytes += lMeta.Bytes;

            return true;
        }

        // Main thread: runs Initialize and marks the slot Loaded, or abandons it if nothing
        // references it anymore.
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
                AbandonSlot(InSlot); // orphaned
                return;
            }
            MaybeInitialize(PayloadRef(InSlot).value()); // main thread
            lMeta.State = EResourceState::Loaded;
            --m_LoadingCount;
            ++m_LoadedCount;
            m_TotalBytes += lMeta.Bytes;
        }

        // Discards a Loading slot (failed or orphaned); bumps the generation.
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
            PayloadRef(InSlot).reset(); // may release children
        }


        /**
         * Frame-stable pointer. Never null for Placeholder types.
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
         * @return True if the handle points at the current, loaded occupant of its slot
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
                // Deferred unload: the payload stays valid until the next CollectGarbage(), and a Load
                // before then brings it back. A Loading slot has no payload; FinalizeSlot abandons it.
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

        // State of a handle's slot; Unloaded if out of range or stale.
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
                PayloadRef(lSlot).reset(); // may release children on other pools
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
         * Destroys the slots released before this call. Works on a snapshot: children released while
         * unloading are handled next time. Reused slots are skipped.
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
                if (lMeta.RefCount != 0)                     { continue; } // referenced again
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

        // Stable address: the chunk never moves.
        SlotMeta&       MetaRef(Uint32 InSlot)       noexcept { return (*m_MetaChunks[InSlot / ChunkSize])[InSlot % ChunkSize]; }
        const SlotMeta& MetaRef(Uint32 InSlot) const noexcept { return (*m_MetaChunks[InSlot / ChunkSize])[InSlot % ChunkSize]; }

        /**
         * Meta of a live handle (valid, in range, current generation, Loaded), else null.
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

        // Meta for refcounting: accepts Loaded or Loading (refs to async loads can be copied before publish).
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
                return lSlot; // keeps its bumped generation
            }
            const Uint32 lSlot = m_SlotCount++;
            if (lSlot / ChunkSize >= m_Chunks.size())
            {
                // New chunk (payload + meta). Must stay within MaxChunks or Resolve is no longer lock-free.
                OPAAX_ASSERT(m_Chunks.size() < MaxChunks) // MaxChunks exceeded
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

            // Bump the generation before destroying, so a re-entrant Get sees the slot as gone.
            m_TotalBytes -= lMeta.Bytes;
            --m_LoadedCount;
            m_PathToSlot.erase(lSourceKey);

            ++lMeta.Generation;
            lMeta.State    = EResourceState::Unloaded;
            lMeta.RefCount = 0;
            lMeta.Bytes    = 0;
            lMeta.Source   = OpaaxStringID{};
            m_FreeSlots.emplace_back(InSlot);

            // Destroy last: children release on their own pools.
            PayloadRef(InSlot).reset();
        }
        /***/
        T* PlaceholderOrNull() noexcept
        {
            if constexpr (T::FailPolicy == EFailPolicy::Placeholder)
            {
                if (!m_Placeholder.has_value())
                {
                    // The placeholder is fully initialized (Initialize runs), so a GPU type has its texture.
                    // Built on first use, on the main thread.
                    m_Placeholder.emplace(T::Placeholder());
                    MaybeInitialize(m_Placeholder.value());
                }

                return &m_Placeholder.value();
            }
            else
            {
                return nullptr; // FailFast: null
            }
        }
        /***/
        static void MaybeInitialize(T& InValue) noexcept
        {
            // Main-thread Initialize (GPU upload), if the type has one.
            if constexpr (requires(T& v) { v.Initialize(); })
            {
                InValue.Initialize();
            }
        }

        static Uint64 ComputeBytes(const T& InValue) noexcept
        {
            // Use ByteSize() if the type has it, else the struct size.
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
        TDynArray<TUniquePtr<Chunk>>     m_Chunks;      // stable payload storage
        TDynArray<TUniquePtr<MetaChunk>> m_MetaChunks;  // stable slot metadata
        Uint32                          m_SlotCount = 0; // highest slot index used
        TDynArray<Uint32>               m_FreeSlots;   // reusable slots
        TDynArray<GraveEntry>           m_Graveyard;   // released slots, destroyed next Update
        TUnorderedMap<Uint32, Uint32>    m_PathToSlot;  // path id -> slot
        std::optional<T>                m_Placeholder; // built on first use
        Uint32                          m_LoadedCount  = 0;
        Uint32                          m_LoadingCount = 0; // async slots in flight
        Uint64                          m_TotalBytes   = 0;
    };
}
