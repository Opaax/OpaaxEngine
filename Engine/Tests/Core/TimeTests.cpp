// Suite: Clock (steady time), DateTime (the wall clock's local date and time), Random (system
// seeded bits) and Process::RunCommandLine (a shell command's exit code, its UTF-8 text).
#include <doctest.h>

#include "Core/IO/FileSystem.h"
#include "Core/Maths/Maths.h"
#include "Core/Maths/Random.h"
#include "Core/OpaaxTypes.h"
#include "Core/Time/Clock.h"
#include "Core/Time/DateTime.h"
#include "Platform/Process.h"

#include "../TestTempDir.h"

using namespace Opaax;

TEST_CASE("Clock: never goes back, and seconds agree with nanoseconds")
{
    const Uint64 lFirstNs  = Clock::NowNanoseconds();
    const double lFirstSec = Clock::NowSeconds();

    ThisThread::SleepFor(Milliseconds(5));

    const Uint64 lSecondNs  = Clock::NowNanoseconds();
    const double lSecondSec = Clock::NowSeconds();

    CHECK(lSecondNs >= lFirstNs);
    CHECK(lSecondSec >= lFirstSec);

    // The sleep is at least 5 ms long, on both readings.
    CHECK(lSecondNs - lFirstNs >= 5'000'000u);
    CHECK(lSecondSec - lFirstSec >= 0.005);

    // The two are the same clock, give or take what passed between the calls.
    CHECK(Maths::Abs(static_cast<double>(lSecondNs) / 1.e9 - lSecondSec) < 0.01);
}

TEST_CASE("DateTime::Now: a valid local date and time")
{
    const DateTime lNow = DateTime::Now();

    CHECK(lNow.Year >= 2024);
    CHECK(lNow.Month >= 1);
    CHECK(lNow.Month <= 12);
    CHECK(lNow.Day >= 1);
    CHECK(lNow.Day <= 31);
    CHECK(lNow.Hour >= 0);
    CHECK(lNow.Hour <= 23);
    CHECK(lNow.Minute >= 0);
    CHECK(lNow.Minute <= 59);
    CHECK(lNow.Second >= 0);
    CHECK(lNow.Second <= 60);   // a leap second
    CHECK(lNow.Millisecond >= 0);
    CHECK(lNow.Millisecond <= 999);
}

TEST_CASE("Random::NextUint64: different bits each call and on each thread")
{
    TUnorderedSet<Uint64> lSeen;
    for (Int32 lIndex = 0; lIndex < 64; ++lIndex)
    {
        lSeen.insert(Random::NextUint64());
    }
    CHECK(lSeen.size() == 64u);

    // Another thread has its own generator, seeded apart.
    Uint64 lOther = 0;
    Thread lWorker([&lOther]() { lOther = Random::NextUint64(); });
    lWorker.join();
    CHECK(lSeen.count(lOther) == 0u);
}

TEST_CASE("Process::RunCommandLine: the command's exit code, on every platform")
{
    CHECK(Process::RunCommandLine("exit 0") == 0);
    CHECK(Process::RunCommandLine("exit 3") == 3);
}

TEST_CASE("Process::RunCommandLine: a path with characters no code page holds reaches the shell whole")
{
    // Two CJK characters and an accented e, in UTF-8: an export into such a folder must work.
    const TestTempDir lTemp("Process");
    const OpaaxString lName = "\xE6\x97\xA5\xE6\x9C\xAC Zo\xC3\xA9";

#ifdef OPAAX_PLATFORM_WINDOWS
    const OpaaxString lLine = OpaaxString::Format("cd /d \"{}\" && mkdir \"{}\"", lTemp.Path(), lName);
#else
    const OpaaxString lLine = OpaaxString::Format("cd \"{}\" && mkdir \"{}\"", lTemp.Path(), lName);
#endif

    CHECK(Process::RunCommandLine(lLine) == 0);
    CHECK(FileSystem::IsDirectory(lTemp.Sub(lName)));
}
