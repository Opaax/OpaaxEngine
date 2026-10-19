#pragma once

#include <cstring>

#include "Core/OpaaxTypes.h"

// =============================================================================
// Memory — raw byte operations: the engine's names for memcpy, memmove, memset and memcmp.
// =============================================================================
namespace Opaax::Memory
{
    /** Copies InSize bytes. The two ranges must not overlap: CopyOverlapping when they may. */
    inline void Copy(void* OutDestination, const void* InSource, const Uint64 InSize) noexcept
    {
        std::memcpy(OutDestination, InSource, static_cast<size_t>(InSize));
    }

    /** Copies InSize bytes; the two ranges may overlap. */
    inline void CopyOverlapping(void* OutDestination, const void* InSource, const Uint64 InSize) noexcept
    {
        std::memmove(OutDestination, InSource, static_cast<size_t>(InSize));
    }

    /** Sets InSize bytes to InValue. */
    inline void Set(void* OutDestination, const Uint8 InValue, const Uint64 InSize) noexcept
    {
        std::memset(OutDestination, InValue, static_cast<size_t>(InSize));
    }

    /** Sets InSize bytes to 0 (a float or an integer of all-zero bytes is 0). */
    inline void Zero(void* OutDestination, const Uint64 InSize) noexcept
    {
        std::memset(OutDestination, 0, static_cast<size_t>(InSize));
    }

    /** @return Below 0, 0 or above 0 as the first differing byte of InA is below, equal or above InB's */
    inline Int32 Compare(const void* InA, const void* InB, const Uint64 InSize) noexcept
    {
        return std::memcmp(InA, InB, static_cast<size_t>(InSize));
    }
}
