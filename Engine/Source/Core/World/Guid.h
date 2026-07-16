#pragma once

#include <functional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // Guid — 128-bit stable identity for entities (and any persistent object). Two
    //   64-bit halves; the all-zero value is the invalid sentinel. New() mints a
    //   random, guaranteed-non-zero value — generation lives out-of-line so <random>
    //   never leaks into this header.
    // =============================================================================
    struct OPAAX_API Guid
    {
        // =========================================================================
        // Data
        // =========================================================================
        Uint64 High = 0;
        Uint64 Low  = 0;

        // =========================================================================
        // Query
        // =========================================================================
        bool IsValid() const noexcept { return (High | Low) != 0; }

        // =========================================================================
        // Compare
        // =========================================================================
        bool operator==(const Guid& InOther) const noexcept
        {
            return High == InOther.High && Low == InOther.Low;
        }
        bool operator!=(const Guid& InOther) const noexcept { return !(*this == InOther); }

        // =========================================================================
        // Factory
        // =========================================================================
        static Guid New() noexcept; // random 128-bit, never zero
    };
}

// =============================================================================
// std::hash<Guid> — lets Guid be an UnorderedMap / unordered_set key.
// =============================================================================
template<>
struct std::hash<Opaax::Guid>
{
    std::size_t operator()(const Opaax::Guid& InGuid) const noexcept
    {
        // Mix both halves (hash_combine with the 64-bit golden-ratio constant).
        Opaax::Uint64 lSeed = InGuid.High;
        lSeed ^= InGuid.Low + 0x9e3779b97f4a7c15ULL + (lSeed << 6) + (lSeed >> 2);
        return static_cast<std::size_t>(lSeed);
    }
};
