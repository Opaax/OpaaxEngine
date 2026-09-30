// Suite: ECS micro-benchmarks on the World/Entity API (spawning bullets, integrating motion).
// Skipped by default. Run in Release with: build.bat bench
//   -> OpaaxTests.exe --test-suite=perf --no-skip=true
// Budgets are loose (see PerfBench.h): they catch algorithmic and allocation regressions.
#include <doctest.h>

#include "World/World.h"
#include "World/Entity/Entity.h"

#include "PerfBench.h"

using namespace Opaax;

namespace
{
    // A representative "bullet": position + velocity POD. Value type, no vtable.
    struct BenchBullet
    {
        float X = 0.f, Y = 0.f, VX = 1.f, VY = 1.f;
    };
}

TEST_SUITE("perf" * doctest::skip())
{
    TEST_CASE("perf: spawn 10k entities + Add<BenchBullet>")
    {
        constexpr std::uint64_t kN = 10'000;

        const Perf::Result lResult = Perf::Measure(kN, 15, []
        {
            World lWorld("Bench");
            for (std::uint64_t lI = 0; lI < kN; ++lI)
            {
                Entity lEntity = lWorld.CreateEntity("b");
                lEntity.Add<BenchBullet>();
            }
        });

        Perf::ReportAndGate("spawn entity+component", lResult, /*ns/op*/ 2000.0);
    }

    TEST_CASE("perf: integrate 100k BenchBullet via World::Each")
    {
        constexpr std::uint64_t kN = 100'000;

        // Build the world ONCE (not part of the timed body — we measure iteration, not spawn).
        World lWorld("Bench");
        for (std::uint64_t lI = 0; lI < kN; ++lI)
        {
            Entity lEntity = lWorld.CreateEntity("b");
            lEntity.Add<BenchBullet>();
        }

        const Perf::Result lResult = Perf::Measure(kN, 25, [&lWorld]
        {
            lWorld.Each<BenchBullet>([](BenchBullet& InBullet)
            {
                InBullet.X += InBullet.VX;
                InBullet.Y += InBullet.VY;
            });
        });

        Perf::ReportAndGate("integrate via Each", lResult, /*ns/op*/ 200.0);
    }
}
