#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceHandle<T> — identity. 8-byte trivially-copyable POD.
//
//   This is the ONLY thing the rest of the engine stores for a resource — it lives
//   in DATA (entt components, save files, network). It never extends lifetime.
//   The generation counter is the safety mechanism: unloading a slot bumps it, so
//   every stale handle in the wild resolves safely (placeholder / null) instead of
//   dangling. Templated on T for type-safety only — it stores no T.
//
//   Team rule: "in code it's a Ref, in data it's a Handle." Bridge a handle back to
//   a lifetime claim with ResourceManager::Pin(handle) -> ResourceRef<T>.
// =============================================================================
namespace Opaax
{
    template<typename T>
    struct ResourceHandle final
    {
        // NOTE: fields are public — this is a POD identity that serializes as-is.
        static constexpr Uint32 InvalidSlot = 0xFFFFFFFFu;

        Uint32 Slot       = InvalidSlot;
        Uint32 Generation = 0;

        // -------------------------------------------------------------------------
        constexpr bool IsValid() const noexcept { return Slot != InvalidSlot; }

        constexpr bool operator==(const ResourceHandle& InOther) const noexcept
        {
            return Slot == InOther.Slot && Generation == InOther.Generation;
        }
        constexpr bool operator!=(const ResourceHandle& InOther) const noexcept
        {
            return !(*this == InOther);
        }
    };
}
