#include "Core/Time/DateTime.h"

#include <chrono>
#include <ctime>

#include "Core/EngineAPI.h"

namespace Opaax
{
    DateTime DateTime::Now()
    {
        using namespace std::chrono;

        const system_clock::time_point lNow     = system_clock::now();
        const std::time_t              lSeconds = system_clock::to_time_t(lNow);

        std::tm lLocal{};
#if defined(OPAAX_PLATFORM_WINDOWS)
        localtime_s(&lLocal, &lSeconds);
#else
        localtime_r(&lSeconds, &lLocal);
#endif

        DateTime lResult;
        lResult.Year        = lLocal.tm_year + 1900;
        lResult.Month       = lLocal.tm_mon + 1;
        lResult.Day         = lLocal.tm_mday;
        lResult.Hour        = lLocal.tm_hour;
        lResult.Minute      = lLocal.tm_min;
        lResult.Second      = lLocal.tm_sec;
        lResult.Millisecond = static_cast<Int32>(duration_cast<milliseconds>(lNow.time_since_epoch()).count() % 1000);
        return lResult;
    }
}
