#include "Core/Maths/Random.h"

#include <random>

namespace Opaax::Random
{
    Uint64 NextUint64() noexcept
    {
        static thread_local std::mt19937_64 s_Engine{ std::random_device{}() };
        std::uniform_int_distribution<Uint64> lDistribution;
        return lDistribution(s_Engine);
    }
}
