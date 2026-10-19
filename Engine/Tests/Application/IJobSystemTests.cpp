// Suite: IJobSystem worker pool. Exercises the real JobSystem (Submit/Wait, ParallelFor,
// deferred OnComplete via DrainCompletions) plus the null object, which runs every job
// inline on the calling thread.
#include <doctest.h>

#include "Application/Services/IJobSystem.h"
#include "Application/Services/AppServiceLocator.h"
#include "Core/OpaaxTypes.h"

using namespace Opaax;

TEST_CASE("JobSystem: Submit runs the work; Wait blocks until it has run")
{
    JobSystem lJobs;
    CHECK(lJobs.GetWorkerCount() >= 1);

    TAtomic<bool> lRan{false};
    JobHandle lHandle = lJobs.Submit([&lRan] { lRan.store(true, EMemoryOrder::Release); });

    lJobs.Wait(lHandle);
    CHECK(lRan.load(EMemoryOrder::Acquire));
    CHECK(lHandle.IsComplete());
}

TEST_CASE("JobSystem: OnComplete is deferred until DrainCompletions (main-thread marshalling)")
{
    JobSystem lJobs;

    TAtomic<bool> lWorkRan{false};
    TAtomic<bool> lCompleteRan{false};

    JobHandle lHandle = lJobs.Submit(
        [&lWorkRan]     { lWorkRan.store(true, EMemoryOrder::Release); },
        [&lCompleteRan] { lCompleteRan.store(true, EMemoryOrder::Release); });

    lJobs.Wait(lHandle);
    CHECK(lWorkRan.load(EMemoryOrder::Acquire));
    CHECK_FALSE(lCompleteRan.load(EMemoryOrder::Acquire)); // never runs without a drain

    // The worker pushes OnComplete just after Wait wakes; drain until it lands.
    for (int i = 0; i < 2000 && !lCompleteRan.load(EMemoryOrder::Acquire); ++i)
    {
        lJobs.DrainCompletions();
        ThisThread::YieldNow();
    }
    CHECK(lCompleteRan.load(EMemoryOrder::Acquire));
}

TEST_CASE("JobSystem: ParallelFor covers every index in [0, count)")
{
    JobSystem lJobs;

    const Uint32 lCount = 1000;
    TDynArray<int> lHits;
    lHits.resize(lCount, 0);

    // Each index is written by exactly one chunk — distinct elements, no data race.
    lJobs.ParallelFor(lCount, [&lHits](Uint32 InIndex) { lHits[InIndex] = 1; }, 64);

    int lSum = 0;
    for (Uint32 i = 0; i < lCount; ++i) { lSum += lHits[i]; }
    CHECK(lSum == static_cast<int>(lCount));
}

TEST_CASE("IJobSystem: the null system runs jobs inline and is never null")
{
    AppServiceLocator lLocator;
    IJobSystem& lSys = lLocator.Get<IJobSystem>(); // unprovided -> NullJobSystem
    CHECK(lSys.IsNull());
    CHECK(&lSys == &IJobSystem::Null());
    CHECK(lSys.GetWorkerCount() == 0);

    // Inline: work + OnComplete both run on the calling thread, no Wait / DrainCompletions.
    TAtomic<bool> lWorkRan{false};
    TAtomic<bool> lCompleteRan{false};
    lSys.Submit(
        [&lWorkRan]     { lWorkRan.store(true, EMemoryOrder::Release); },
        [&lCompleteRan] { lCompleteRan.store(true, EMemoryOrder::Release); });

    CHECK(lWorkRan.load(EMemoryOrder::Acquire));
    CHECK(lCompleteRan.load(EMemoryOrder::Acquire));
}
