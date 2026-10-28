#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // RandomStream — random numbers that replay: the same seed gives the same numbers, on every
    //   platform (which sound variation plays, a frame pacing's jitter). PCG32. For numbers nobody
    //   may predict (identities), Random instead.
    // =============================================================================
    class RandomStream
    {
    public:
        explicit RandomStream(const Uint64 InSeed = 0x853C49E6748FEA9Bull) noexcept { Seed(InSeed); }

        void Seed(const Uint64 InSeed) noexcept
        {
            m_State = 0;
            NextUint32();
            m_State += InSeed;
            NextUint32();
        }

        Uint32 NextUint32() noexcept
        {
            const Uint64 lOld = m_State;
            m_State = lOld * 6364136223846793005ull + INCREMENT;

            const Uint32 lXorShifted = static_cast<Uint32>(((lOld >> 18u) ^ lOld) >> 27u);
            const Uint32 lRotation   = static_cast<Uint32>(lOld >> 59u);
            return (lXorShifted >> lRotation) | (lXorShifted << ((32u - lRotation) & 31u));
        }

        /** In [0, 1). */
        float NextFloat() noexcept
        {
            return static_cast<float>(NextUint32() >> 8) * (1.f / 16777216.f);
        }

        /** In [InMin, InMax). */
        float Range(const float InMin, const float InMax) noexcept
        {
            return InMin + (InMax - InMin) * NextFloat();
        }

        /** In [0, InCount); 0 when InCount is 0. */
        Uint32 Below(const Uint32 InCount) noexcept
        {
            return InCount > 0 ? static_cast<Uint32>((static_cast<Uint64>(NextUint32()) * InCount) >> 32) : 0u;
        }

    private:
        static constexpr Uint64 INCREMENT = 1442695040888963407ull;

        Uint64 m_State = 0;
    };
}
