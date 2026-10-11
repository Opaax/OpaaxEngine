#pragma once

#include <functional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // Guid — 128-bit stable identity for entities and other saved objects.
    //   All zero is invalid. New() returns a random non-zero value.
    // =============================================================================
    struct Guid
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

        /**
         * Deterministic guid for one entity of a prefab instance: a hash of the instance and
         * template guids. Lets the same prefab be instantiated several times, and gives the
         * same result on every run.
         * @return A valid (non-zero) Guid
         */
        static Guid Derive(const Guid& InInstance, const Guid& InTemplate) noexcept;

        // =========================================================================
        // Text form (used on disk)
        // =========================================================================
        /**
         * @return 32 lowercase hex characters, High then Low, no dashes.
         *   An invalid Guid gives 32 zeros.
         */
        OpaaxString ToString() const;

        /**
         * Parses the ToString() form (any case). OutGuid is untouched on failure.
         * @return False if InText is not exactly 32 hex characters
         */
        static bool FromString(const OpaaxString& InText, Guid& OutGuid) noexcept;
    };
}

// =============================================================================
// std::hash<Guid>, for use as an unordered container key.
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
