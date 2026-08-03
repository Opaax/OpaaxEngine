#pragma once

#include <functional>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

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

        // =========================================================================
        // Text form — the ONE way a Guid is written to disk (M5)
        // =========================================================================
        /**
         * @return 32 lowercase hex characters, High then Low. No dashes: this is the engine's
         *   own format, not RFC 4122, and a fixed-width run of digits is what makes FromString
         *   a length check plus one pass.
         *
         * An INVALID Guid stringifies to 32 zeros rather than to an empty string, so the text
         * form is total — every Guid has one, and a reader never has to special-case a blank.
         */
        OpaaxString ToString() const;

        /**
         * Parse the form ToString() produces. Accepts upper or lower case; rejects anything
         * else — wrong length, a non-hex character, dashes.
         *
         * REJECTION LEAVES OutGuid UNTOUCHED, so a failed parse cannot half-overwrite the
         * caller's identity with the digits it managed to read before giving up.
         *
         * @return false if InText is not exactly 32 hex characters.
         */
        static bool FromString(const OpaaxString& InText, Guid& OutGuid) noexcept;
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
