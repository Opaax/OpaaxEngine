// Suite: the M5 DISK layer — MapFile (.opaaxmap read/write) and MapResource (the CResource).
//
// The headline case is the FULL ROUND TRIP THROUGH DISK: World -> Capture -> Save -> load as a
// MapResource -> Instantiate -> an equivalent world with GUIDs preserved. MapSnapshotTests proves
// that round trip in memory; this proves the two layers M5 adds under it do not lose anything on
// the way to a file and back, which is the only version of the claim an author cares about.
//
// Runs against a UNIQUE directory under the OS temp dir, created and removed per case — the suite
// never touches the repo, and never the editor's own files ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Core/IO/FileIO.h"
#include "Core/String/OpaaxUtf8.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"   // before MapResource — completes LoadContext
#include "World/ComponentRegistry.h"
#include "World/Components/DummyComponent.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Serialization/MapFactory.h"
#include "World/Serialization/MapFile.h"
#include "World/Serialization/MapResource.hpp"
#include "World/Serialization/MapSerializer.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // One temp directory per case, removed on scope exit — including when a CHECK throws.
    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxMapFileTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);          // a previous crashed run must not poison this one
            fs::create_directories(m_Path, lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        OpaaxString Sub(const char* InRel) const
        {
            return OpaaxString((m_Path / InRel).generic_string().c_str());
        }

    private:
        fs::path m_Path;
    };

    // A component defined exe-side, as a game module's would be — the same shape
    // MapSnapshotTests uses, so this suite proves module types survive the file too.
    struct StatsComponent
    {
        int   Health = 0;
        float Speed  = 0.f;

        bool operator==(const StatsComponent& InOther) const
        {
            return Health == InOther.Health && Speed == InOther.Speed;
        }
    };

    inline void to_json(nlohmann::json& InJson, const StatsComponent& InValue)
    {
        InJson = nlohmann::json{{"Health", InValue.Health}, {"Speed", InValue.Speed}};
    }

    inline void from_json(const nlohmann::json& InJson, StatsComponent& InValue)
    {
        InJson.at("Health").get_to(InValue.Health);
        InJson.at("Speed").get_to(InValue.Speed);
    }

    void FillRegistry(ComponentRegistry& InRegistry)
    {
        REQUIRE(InRegistry.Register<DummyComponent>("Dummy"));
        REQUIRE(InRegistry.Register<StatsComponent>("Stats"));
    }

    // Loads a map the way the engine will: through the ResourceManager, as a MapResource.
    // Returns an empty MapData when the load failed (a FailFast handle resolves to null).
    MapData LoadThroughResourceManager(ResourceManager& InResources, const OpaaxString& InPath, bool& bOutLoaded)
    {
        const ResourceRef<MapResource> lRef = InResources.Load<MapResource>(InPath.CStr());
        const MapResource* const       lMap = lRef.Get();

        bOutLoaded = (lMap != nullptr);
        return bOutLoaded ? lMap->Data : MapData{};
    }
}

// =============================================================================
// The gate — the round trip through a file
// =============================================================================
TEST_CASE("MapFile: World -> Capture -> Save -> MapResource -> Instantiate rebuilds the world, GUIDs preserved")
{
    const ScopedTempDir lTemp("round_trip");
    const OpaaxString   lPath = lTemp.Sub("Main.opaaxmap");

    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMap = MapId("Level01");

    // ---- author a world ------------------------------------------------------
    World lSource("Authored");

    Entity lHero = lSource.CreateEntity("Hero", lMap);
    lHero.Add<StatsComponent>(StatsComponent{100, 4.5f});
    lHero.Add<DummyComponent>();
    lHero.Get<DummyComponent>().Position = Vector2F{12.f, -3.f};

    Entity lCrate = lSource.CreateEntity("Crate", lMap);
    lCrate.Add<DummyComponent>();
    lCrate.Get<DummyComponent>().Color = Vector4F{0.25f, 0.5f, 0.75f, 1.f};

    const Guid lHeroId  = lHero.GetGuid();
    const Guid lCrateId = lCrate.GetGuid();

    // ---- save ----------------------------------------------------------------
    REQUIRE(MapFile::Save(lPath, MapSerializer::Capture(lSource, lRegistry, lMap)));

    // ---- load, through the ResourceManager, into a DIFFERENT world ------------
    ResourceManager lResources;
    bool            lLoaded = false;
    const MapData   lData   = LoadThroughResourceManager(lResources, lPath, lLoaded);
    REQUIRE(lLoaded);

    World lRestored("Restored");
    CHECK(MapFactory::Instantiate(lData, lRestored, lRegistry) == 2);

    // ---- the identity claim --------------------------------------------------
    Entity lRestoredHero = lRestored.FindByGuid(lHeroId);
    REQUIRE(lRestoredHero.IsValid());
    CHECK(lRestoredHero.Get<EntityMeta>().Name == OpaaxString("Hero"));
    CHECK(lRestoredHero.Get<EntityMeta>().OwnerMap == lMap);
    CHECK(lRestoredHero.Get<StatsComponent>() == StatsComponent{100, 4.5f});
    CHECK(lRestoredHero.Get<DummyComponent>().Position.x == doctest::Approx(12.f));
    CHECK(lRestoredHero.Get<DummyComponent>().Position.y == doctest::Approx(-3.f));

    Entity lRestoredCrate = lRestored.FindByGuid(lCrateId);
    REQUIRE(lRestoredCrate.IsValid());
    CHECK(lRestoredCrate.Get<EntityMeta>().Name == OpaaxString("Crate"));
    CHECK(lRestoredCrate.Get<DummyComponent>().Color.z == doctest::Approx(0.75f));

    lResources.FlushAll();
}

TEST_CASE("MapFile: a filtered save writes the MAP's entities and leaves runtime-spawned ones out")
{
    // WM2's rule finally exercised against a file: a runtime-spawned entity carries an invalid
    // OwnerMap and so can never match a valid filter. This is what keeps bullets and VFX out of
    // a saved map WITHOUT a special case anywhere.
    const ScopedTempDir lTemp("filtered");
    const OpaaxString   lPath = lTemp.Sub("Filtered.opaaxmap");

    ComponentRegistry lRegistry;
    FillRegistry(lRegistry);

    const MapId lMap = MapId("Level01");

    World lWorld("Mixed");
    lWorld.CreateEntity("Authored", lMap);
    lWorld.CreateEntity("Bullet");          // no map — runtime-spawned

    REQUIRE(MapFile::Save(lPath, MapSerializer::Capture(lWorld, lRegistry, lMap)));

    MapData lLoaded;
    REQUIRE(MapFile::Load(lPath, lLoaded));

    REQUIRE(lLoaded.EntityCount() == 1);
    CHECK(lLoaded.Entities[0].Name == OpaaxString("Authored"));
}

TEST_CASE("MapFile: Save creates missing parent directories")
{
    // "Save As" into a folder the user just named must not fail because the folder is not there.
    const ScopedTempDir lTemp("create_parents");
    const OpaaxString   lPath = lTemp.Sub("Levels/Sub/Deep.opaaxmap");

    MapData lData;
    EntityData lEntity;
    lEntity.Id   = Guid::New();
    lEntity.Name = OpaaxString("Solo");
    lData.Entities.push_back(Move(lEntity));

    REQUIRE(MapFile::Save(lPath, lData));

    MapData lLoaded;
    REQUIRE(MapFile::Load(lPath, lLoaded));
    CHECK(lLoaded.EntityCount() == 1);
}

TEST_CASE("MapFile: an EMPTY map saves and loads — clearing a world is a thing authors do")
{
    const ScopedTempDir lTemp("empty");
    const OpaaxString   lPath = lTemp.Sub("Empty.opaaxmap");

    REQUIRE(MapFile::Save(lPath, MapData{}));

    MapData lLoaded;
    lLoaded.Entities.push_back(EntityData{});   // must be replaced, not appended to

    REQUIRE(MapFile::Load(lPath, lLoaded));
    CHECK(lLoaded.IsEmpty());
}

// =============================================================================
// Failure — the whole reason this type is FailFast
// =============================================================================
TEST_CASE("MapFile: loading a missing file fails and LEAVES THE CALLER'S MAP UNTOUCHED")
{
    const ScopedTempDir lTemp("missing");

    MapData lExisting;
    lExisting.Entities.push_back(EntityData{});

    CHECK_FALSE(MapFile::Load(lTemp.Sub("NotHere.opaaxmap"), lExisting));

    // If a failed load half-replaced this, the next Save would write the wreckage over the file
    // it could not read.
    CHECK(lExisting.EntityCount() == 1);
}

TEST_CASE("MapFile: malformed contents fail rather than throwing")
{
    const ScopedTempDir lTemp("malformed");
    const OpaaxString   lPath = lTemp.Sub("Broken.opaaxmap");

    REQUIRE(FileIO::WriteAllText(lPath, OpaaxString("{ not a map")));

    MapData lLoaded;
    CHECK_FALSE(MapFile::Load(lPath, lLoaded));
    CHECK(lLoaded.IsEmpty());
}

TEST_CASE("MapResource: a missing map resolves to NULL, never to an empty placeholder map")
{
    // The FailFast decision, stated as a test. A placeholder here would not degrade, it would
    // LIE: the level appears to load, the world comes up empty, and nothing reports a problem.
    const ScopedTempDir lTemp("failfast");

    ResourceManager lResources;

    const ResourceRef<MapResource> lRef = lResources.Load<MapResource>(lTemp.Sub("Absent.opaaxmap").CStr());
    CHECK(lRef.Get() == nullptr);

    lResources.FlushAll();
}

TEST_CASE("MapResource: the same path loads once and is shared")
{
    // Dedup is the reason the level loader goes through the ResourceManager rather than calling
    // MapFile::Load directly — two worlds opening the same map must not parse it twice.
    const ScopedTempDir lTemp("dedup");
    const OpaaxString   lPath = lTemp.Sub("Shared.opaaxmap");

    MapData lData;
    EntityData lEntity;
    lEntity.Id   = Guid::New();
    lEntity.Name = OpaaxString("Shared");
    lData.Entities.push_back(Move(lEntity));
    REQUIRE(MapFile::Save(lPath, lData));

    ResourceManager lResources;

    const ResourceRef<MapResource> lFirst  = lResources.Load<MapResource>(lPath.CStr());
    const ResourceRef<MapResource> lSecond = lResources.Load<MapResource>(lPath.CStr());

    REQUIRE(lFirst.Get() != nullptr);
    CHECK(lFirst.Get() == lSecond.Get());          // one payload, two claims
    CHECK(lResources.GetLoadedCount<MapResource>() == 1);

    lResources.FlushAll();
}

TEST_CASE("MapResource: ByteSize grows with the map")
{
    // It under-counts on purpose (structural records only, not the json payload trees) — what
    // must hold is that it is not the constant sizeof(MapResource) the pool would fall back to.
    MapResource lEmpty;

    MapResource lPopulated;
    for (int lIndex = 0; lIndex < 8; ++lIndex)
    {
        EntityData lEntity;
        lEntity.Id   = Guid::New();
        lEntity.Name = OpaaxString("Entity");
        lEntity.Components.push_back(ComponentData{ OpaaxStringID("Dummy"), nlohmann::json::object() });
        lPopulated.Data.Entities.push_back(Move(lEntity));
    }

    CHECK(lPopulated.ByteSize() > lEmpty.ByteSize());
}

// =============================================================================
// I7 — the path is UTF-8, and the file layer must not decode it as ANSI
// =============================================================================
TEST_CASE("MapFile: a map saves and loads under a NON-ASCII path")
{
    // \uXXXX escapes, never literal characters: this file has no BOM and the build sets no
    // /utf-8, so MSVC would decode literals using the ANSI code page — the exact mechanism under
    // test ([[L21]]). U+65E5 U+672C are outside CP-1252 entirely, so this cannot pass by being
    // consistently wrong.
    namespace fs = std::filesystem;

    const wchar_t* const lWideName = L"\u65E5\u672C_Caf\u00E9";
    const fs::path       lDir      = fs::temp_directory_path() / "OpaaxMapFileTests_utf8" / lWideName;

    std::error_code lError;
    fs::remove_all(lDir.parent_path(), lError);
    fs::create_directories(lDir, lError);

    const OpaaxString lPath = Utf8::FromFsPath(lDir) + OpaaxString("/Main.opaaxmap");

    MapData lData;
    EntityData lEntity;
    lEntity.Id   = Guid::New();
    lEntity.Name = OpaaxString("Accented");
    lData.Entities.push_back(Move(lEntity));

    REQUIRE(MapFile::Save(lPath, lData));

    // Verified through the WIDE API — a different mechanism than the one under test.
    CHECK(fs::exists(lDir / L"Main.opaaxmap"));

    MapData lLoaded;
    REQUIRE(MapFile::Load(lPath, lLoaded));
    REQUIRE(lLoaded.EntityCount() == 1);
    CHECK(lLoaded.Entities[0].Name == OpaaxString("Accented"));

    fs::remove_all(lDir.parent_path(), lError);
}
