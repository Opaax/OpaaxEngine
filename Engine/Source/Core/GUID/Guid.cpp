#include "Guid.h"

#include <random>

namespace Opaax
{
    // =========================================================================
    // New — two 64-bit draws from a thread-local Mersenne engine, seeded once per
    // thread from random_device. Forced non-zero so the invalid sentinel can never
    // be minted by accident.
    // =========================================================================
    Guid Guid::New() noexcept
    {
        static thread_local std::mt19937_64 sEngine{ std::random_device{}() };
        std::uniform_int_distribution<Uint64> lDist;

        Guid lGuid;
        lGuid.High = lDist(sEngine);
        lGuid.Low  = lDist(sEngine);

        if ((lGuid.High | lGuid.Low) == 0)
        {
            lGuid.Low = 1;
        }

        return lGuid;
    }
}
