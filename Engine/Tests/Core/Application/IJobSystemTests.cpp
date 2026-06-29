// Suite: IJobSystem worker pool. Exercises the real JobSystem (Submit/Wait, ParallelFor,
// deferred OnComplete via DrainCompletions) plus the null object, which runs every job
// inline on the calling thread.
#include <doctest.h>

#include <thread>

#include "Core/Application/Services/IJobSystem.h"
#include "Core/Application/Services/AppServiceLocator.h"
#include "Core/OpaaxTypes.h"

using namespace Opaax;

TEST_CASE("JobSystem: Submit runs the work; Wait blocks until it has run")
{
    JobSystem lJobs;
    CHECK(lJobs.GetWorkerCount() >= 1);

    Atomic<bool> lRan{false};
    JobHandle lHandle = lJobs.Submit([&lRan] { lRan.store(true, std::memory_order_release); });

    lJobs.Wait(lHandle);
    CHECK(lRan.load(std::memory_order_acquire));
    CHECK(lHandle.IsComplete());
}

TEST_CASE("JobSystem: OnComplete is deferred until DrainCompletions (main-thread marshalling)")
{
    JobSystem lJobs;

    Atomic<bool> lWorkRan{false};
    Atomic<bool> lCompleteRan{false};

    JobHandle lHandle = lJobs.Submit(
        [&lWorkRan]     { lWorkRan.store(true, std::memory_order_release); },
        [&lCompleteRan] { lCompleteRan.store(true, std::memory_order_release); });

    lJobs.Wait(lHandle);
    CHECK(lWorkRan.load(std::memory_order_acquire));
    CHECK_FALSE(lCompleteRan.load(std::memory_order_acquire)); // never runs without a drain

    // The worker pushes OnComplete just after Wait wakes; drain until it lands.
    for (int i = 0; i < 2000 && !lCompleteRan.load(std::memory_order_acquire); ++i)
    {
        lJobs.DrainCompletions();
        std::this_thread::yield();
    }
    CHECK(lCompleteRan.load(std::memory_order_acquire));
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
    Atomic<bool> lWorkRan{false};
    Atomic<bool> lCompleteRan{false};
    lSys.Submit(
        [&lWorkRan]     { lWorkRan.store(true, std::memory_order_release); },
        [&lCompleteRan] { lCompleteRan.store(true, std::memory_order_release); });

    CHECK(lWorkRan.load(std::memory_order_acquire));
    CHECK(lCompleteRan.load(std::memory_order_acquire));
}
