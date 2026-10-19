#include "Core/Time/Clock.h"

#include <chrono>

namespace Opaax::Clock
{
    double NowSeconds() noexcept
    {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }

    Uint64 NowNanoseconds() noexcept
    {
        using namespace std::chrono;
        return static_cast<Uint64>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    }
}
