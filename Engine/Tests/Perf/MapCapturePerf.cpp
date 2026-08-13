// Suite: the cost of the EDITOR'S DIRTY CHECK — MapSerializer::CaptureMap + MapJson::Serialize.
//
// This is the pass EditorLevelDocument::RefreshDirty runs per mounted map. Capture ALONE would
// understate it by roughly half: the dump(4) that turns MapData into comparable text is part of the
// check, not part of saving.
//
// WHY IT IS MEASURED: the check is gated on World::GetRevision() (MP5), so an idle editor pays
// nothing — but every pass that DOES run costs this, and it grows with the level. The numbers here
// say at what entity count one pass stops fitting beside a frame.
//
// The whole suite is SKIPPED by default. Run in RELEASE via:  build.bat bench
#include <cstdio>
#include <string>

#include <doctest.h>

#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Serialization/MapJson.h"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

#include "PerfBench.h"

using namespace Opaax;

namespace
{
    // Four PODs, shaped like real authoring components: nlohmann by ADL, no base class (I8).
    // FOUR registered on purpose, TWO carried per entity — a capture pays a Has() probe for every
    // registered type on every entity, including the ones it does not have.
    struct BenchTransform
    {
        float X = 1.f, Y = 2.f, Rotation = 3.f, ScaleX = 1.f, ScaleY = 1.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BenchTransform, X, Y, Rotation, ScaleX, ScaleY)
    };

    // std::string, not OpaaxString: the engine has no json bridge for the latter (real components
    // carry math types — see DummyComponent). The point here is that ONE component allocates a
    // string per capture, which the sprite slice will make the common case.
    struct BenchSprite
    {
        std::string Texture = "Textures/Bench.png";
        int         Layer   = 2;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BenchSprite, Texture, Layer)
    };

    struct BenchStats
    {
        int   Health = 100;
        float Speed  = 42.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BenchStats, Health, Speed)
    };

    struct BenchCollider
    {
        float Width = 8.f, Height = 8.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(BenchCollider, Width, Height)
    };

    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<BenchTransform>("BenchTransform"));
        REQUIRE(InRegistry.Register<BenchSprite>("BenchSprite"));
        REQUIRE(InRegistry.Register<BenchStats>("BenchStats"));
        REQUIRE(InRegistry.Register<BenchCollider>("BenchCollider"));
    }

    // One authored map's worth of entities, all claiming InMapId so the filtered capture keeps them.
    void FillWorld(World& InWorld, MapId InMapId, std::uint64_t InCount)
    {
        for (std::uint64_t lIndex = 0; lIndex < InCount; ++lIndex)
        {
            Entity lEntity = InWorld.CreateEntity("BenchEntity", InMapId);
            lEntity.Add<BenchTransform>();
            lEntity.Add<BenchSprite>();
        }
    }

    // ns/op is per ENTITY; a frame budget is per PASS. Print both — the second is the one that
    // answers "how big can a level get before the check is visible?".
    void ReportPass(const char* InLabel, const Perf::Result& InResult, std::uint64_t InEntities,
                    double InBudgetNsPerEntity)
    {
        std::printf("  [perf] %-38s %9.3f ms/pass  (%llu entities)\n", InLabel,
                    InResult.MedianNsPerOp * static_cast<double>(InEntities) / 1'000'000.0,
                    static_cast<unsigned long long>(InEntities));
        std::fflush(stdout);

        Perf::ReportAndGate(InLabel, InResult, InBudgetNsPerEntity);
    }

    void RunCase(const char* InLabel, std::uint64_t InCount, int InEpochs, double InBudgetNsPerEntity)
    {
        const MapId lMapId("BenchMap");

        ComponentRegistry lRegistry;
        FillRegistry(lRegistry);

        World lWorld("Bench");
        FillWorld(lWorld, lMapId, InCount);

        // A sink, not an assertion: a REQUIRE inside the timed body would measure doctest. Summing a
        // byte keeps the optimizer from eliding the work without adding any of its own.
        Uint64 lSink = 0;

        // EXACTLY what RefreshDirty does per mounted map: capture filtered by the map's id, serialize
        // to the text form, and (the caller's part) compare against a baseline string.
        const Perf::Result lResult = Perf::Measure(InCount, InEpochs, [&]
        {
            const OpaaxString lText = MapJson::Serialize(MapSerializer::CaptureMap(lWorld, lRegistry, lMapId));
            lSink += lText.GetLength();
        });

        REQUIRE(lSink > 0);   // the capture really produced text — checked OUTSIDE the timed body

        ReportPass(InLabel, lResult, InCount, InBudgetNsPerEntity);
    }
}

TEST_SUITE("perf" * doctest::skip())
{
    TEST_CASE("perf: dirty check (capture+serialize) — 100 entities")
    {
        RunCase("dirty check 100", 100, 15, /*ns per entity*/ 20'000.0);
    }

    TEST_CASE("perf: dirty check (capture+serialize) — 1k entities")
    {
        RunCase("dirty check 1k", 1'000, 15, /*ns per entity*/ 20'000.0);
    }

    TEST_CASE("perf: dirty check (capture+serialize) — 10k entities")
    {
        RunCase("dirty check 10k", 10'000, 9, /*ns per entity*/ 20'000.0);
    }
}
