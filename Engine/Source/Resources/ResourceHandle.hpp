#pragma once

#include "Core/OpaaxTypes.h"

// =============================================================================
// ResourceHandle<T> — resource identity, 8 bytes, plain data.
//   What data stores (components, save files). Does not keep the resource loaded.
//   A generation counter makes stale handles resolve safely (placeholder / null).
//   In code use a ResourceRef; in data use a Handle. ResourceManager::Pin(handle)
//   turns a handle back into a Ref.
// =============================================================================
namespace Opaax
{
    template<typename T>
    struct ResourceHandle final
    {
        // Public fields: plain data, serialized as is.
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
