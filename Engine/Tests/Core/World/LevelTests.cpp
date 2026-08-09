// Suite: Level — WHICH maps are in a world, and the two verbs that put them there or take them
// out. Covers the MOUNT ORDER WM1a made a rule (persistent first), unmount-as-a-filter (WM2), the
// authoring verbs, and the clone rule (a clone COPIES mount state and must never re-mount).
//
// Runs against a UNIQUE directory under the OS temp dir, created and removed per case — the
// suite never touches the repo ([[L20]]).
#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "Application/Services/IPaths.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Level.h"
#include "World/Serialization/LevelFile.h"
#include "World/World.h"

using namespace Opaax;

namespace
{
    namespace fs = std::filesystem;

    // Every asset lives under one temp root, so AssetToAbsolute is a concatenation. The rest of
    // IPaths answers the root: Level reaches for that ONE resolver and nothing else, and a stub
    // that lied about the others would still never be caught at it.
    class TempPaths final : public IPaths
    {
    public:
        explicit TempPaths(const fs::path& InRoot) : m_Root(InRoot) {}

        OpaaxString AssetToAbsolute(const OpaaxString& InAssetRel) const override
        {
            return OpaaxString((m_Root / InAssetRel.CStr()).generic_string().c_str());
        }

        OpaaxString AbsoluteToAsset(const OpaaxString& InAbsPath) const override
        {
            const fs::path lRelative = fs::path(InAbsPath.CStr()).lexically_relative(m_Root);
            return OpaaxString(lRelative.generic_string().c_str());
        }

        void        LogPaths()      const override {}
        OpaaxString WorkspaceRoot() const override { return Root(); }
        OpaaxString EngineRoot()    const override { return Root(); }
        OpaaxString ProjectRoot()   const override { return Root(); }
        OpaaxString ProjectFile()   const override { return Root(); }
        OpaaxString AssetsDir()     const override { return Root(); }
        OpaaxString ConfigsDir()    const override { return Root(); }
        OpaaxString SourceDir()     const override { return Root(); }
        OpaaxString SaveDir()       const override { return Root(); }
        OpaaxString TempDir()       const override { return Root(); }

        OpaaxString EngineToAbsolute(const OpaaxString& InRel)  const override { return AssetToAbsolute(InRel); }
        OpaaxString ProjectToAbsolute(const OpaaxString& InRel) const override { return AssetToAbsolute(InRel); }

    private:
        OpaaxString Root() const { return OpaaxString(m_Root.generic_string().c_str()); }

        fs::path m_Root;
    };

    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(const char* InTag)
        {
            m_Path = fs::temp_directory_path() / ("OpaaxLevelTests_" + std::string(InTag));

            std::error_code lError;
            fs::remove_all(m_Path, lError);
            fs::create_directories(m_Path / "Maps", lError);
        }

        ~ScopedTempDir()
        {
            std::error_code lError;
            fs::remove_all(m_Path, lError);
        }

        ScopedTempDir(const ScopedTempDir&)            = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        void Write(const char* InAssetRel, const std::string& InText) const
        {
            std::ofstream lOut(m_Path / InAssetRel, std::ios::binary);
            lOut << InText;
        }

        const fs::path& Root() const { return m_Path; }

    private:
        fs::path m_Path;
    };

    // A one-entity map, hand-written: MapFile/MapJson have their own suites, so this only has to
    // be READABLE — no component payloads, which is also why a bare ComponentRegistry serves.
    std::string MapText(const char* InGuid, const char* InName, const char* InOwnerMap)
    {
        return std::string(R"({"version":1,"entities":[{"guid":")") + InGuid +
               R"(","name":")" + InName +
               R"(","ownerMap":")" + InOwnerMap +
               R"(","components":{}}]})";
    }

    constexpr const char* SHARED_GUID = "0123456789abcdef0123456789abcdef";

    // The four references a Level is built from, bundled so a case reads as its own story.
    struct Fixture
    {
        ScopedTempDir     Dir;
        World             TheWorld;
        ComponentRegistry Components;
        ResourceManager   Resources;
        TempPaths         Paths;
        Level             TheLevel;

        explicit Fixture(const char* InTag)
            : Dir(InTag)
            , TheWorld(OpaaxString(InTag))
            , Paths(Dir.Root())
            , TheLevel(TheWorld, Components, Paths, Resources)
        {
        }
    };
}

TEST_CASE("Level: the PERSISTENT map is mounted first, whatever its place in the manifest")
{
    // Order is proven by a COLLISION, not by iteration order. Both maps carry the same Guid, and
    // World::CreateEntityWithGuid REFUSES one already live (WM3) — so whichever name survives is
    // whichever map arrived first, and entt's storage order never enters into the answer.
    Fixture lFix("order");
    lFix.Dir.Write("Maps/Persistent.opaaxmap", MapText(SHARED_GUID, "FromPersistent", "Persistent"));
    lFix.Dir.Write("Maps/Decor.opaaxmap",      MapText(SHARED_GUID, "FromDecor",      "Decor"));

    LevelData lData;
    lData.Name = OpaaxString("Order");
    lData.Maps.push_back(OpaaxString("Maps/Decor.opaaxmap"));        // FIRST in the array...
    lData.Maps.push_back(OpaaxString("Maps/Persistent.opaaxmap"));
    lData.PersistentMapIndex = 1;                                    // ...but SECOND is persistent
    lFix.TheLevel.SetData(lData);

    const Level::MountResult lResult = lFix.TheLevel.MountAll();

    CHECK(lResult.MapsMounted == 2);
    CHECK(lResult.MapsFailed == 0);
    CHECK(lResult.EntitiesCreated == 1);   // the second arrival was refused the live Guid

    Guid lShared;
    REQUIRE(Guid::FromString(OpaaxString(SHARED_GUID), lShared));

    Entity lSurvivor = lFix.TheWorld.FindByGuid(lShared);
    REQUIRE(lSurvivor.IsValid());
    CHECK(lSurvivor.Get<EntityMeta>().Name == OpaaxString("FromPersistent"));
}

TEST_CASE("Level: every map of the level arrives, persistent or not")
{
    Fixture lFix("all");
    lFix.Dir.Write("Maps/A.opaaxmap", MapText("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "InA", "A"));
    lFix.Dir.Write("Maps/B.opaaxmap", MapText("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "InB", "B"));
    lFix.Dir.Write("Maps/C.opaaxmap", MapText("cccccccccccccccccccccccccccccccc", "InC", "C"));

    LevelData lData;
    lData.Name = OpaaxString("All");
    lData.Maps.push_back(OpaaxString("Maps/A.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/B.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/C.opaaxmap"));
    lData.PersistentMapIndex = 1;
    lFix.TheLevel.SetData(lData);

    const Level::MountResult lResult = lFix.TheLevel.MountAll();

    // The persistent map is mounted out of turn, NOT twice and not instead of the rest — the
    // skip-by-index is the half of the loop that is easy to get wrong.
    CHECK(lResult.MapsMounted == 3);
    CHECK(lResult.EntitiesCreated == 3);
    CHECK(lFix.TheWorld.GetEntityCount() == 3);
    CHECK(lFix.TheLevel.GetMountedMaps().size() == 3);
    CHECK(lResult.IsValid());

    // Mount order, plainly: persistent first, then manifest order around it.
    CHECK(lFix.TheLevel.GetMountedMaps()[0].AssetRelPath == OpaaxString("Maps/B.opaaxmap"));
    CHECK(lFix.TheLevel.GetPersistentMapId() == MapId("B"));
}

TEST_CASE("Level: a missing map costs that map, not the level")
{
    Fixture lFix("missing");
    lFix.Dir.Write("Maps/Here.opaaxmap", MapText("11111111111111111111111111111111", "Here", "Here"));

    LevelData lData;
    lData.Name = OpaaxString("Missing");
    lData.Maps.push_back(OpaaxString("Maps/Here.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/Gone.opaaxmap"));
    lFix.TheLevel.SetData(lData);

    const Level::MountResult lResult = lFix.TheLevel.MountAll();

    CHECK(lResult.MapsMounted == 1);
    CHECK(lResult.MapsFailed == 1);
    CHECK(lResult.EntitiesCreated == 1);
    CHECK_FALSE(lResult.IsValid());   // something arrived, but not everything — not a success
}

TEST_CASE("Level: an empty level touches nothing and reports itself invalid")
{
    // The case an empty world cannot be told apart from on its own, which is the whole reason
    // MountResult exists rather than a bool.
    Fixture lFix("empty");

    const Level::MountResult lResult = lFix.TheLevel.MountAll();

    CHECK(lResult.MapsMounted == 0);
    CHECK(lResult.MapsFailed == 0);
    CHECK(lFix.TheWorld.GetEntityCount() == 0);
    CHECK_FALSE(lResult.IsValid());
}

TEST_CASE("Level: unmounting destroys exactly that map's entities")
{
    // WM2's partition, exercised: the entities of map X are a FILTER over one registry, so
    // unmounting must take those and leave every other map standing.
    Fixture lFix("unmount");
    lFix.Dir.Write("Maps/Keep.opaaxmap", MapText("11111111111111111111111111111111", "Kept", "Keep"));
    lFix.Dir.Write("Maps/Drop.opaaxmap", MapText("22222222222222222222222222222222", "Dropped", "Drop"));

    LevelData lData;
    lData.Name = OpaaxString("Unmount");
    lData.Maps.push_back(OpaaxString("Maps/Keep.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/Drop.opaaxmap"));
    lFix.TheLevel.SetData(lData);

    REQUIRE(lFix.TheLevel.MountAll().MapsMounted == 2);
    REQUIRE(lFix.TheWorld.GetEntityCount() == 2);

    CHECK(lFix.TheLevel.Unmount(MapId("Drop")));

    CHECK(lFix.TheWorld.GetEntityCount() == 1);
    CHECK_FALSE(lFix.TheLevel.IsMounted(MapId("Drop")));
    CHECK(lFix.TheLevel.IsMounted(MapId("Keep")));

    // The survivor is the OTHER map's entity, not merely "an" entity.
    Guid lKept;
    REQUIRE(Guid::FromString(OpaaxString("11111111111111111111111111111111"), lKept));
    CHECK(lFix.TheWorld.FindByGuid(lKept).IsValid());

    // The MANIFEST is untouched: unmount is a runtime verb, RemoveMap is the authoring one.
    CHECK(lFix.TheLevel.GetData().MapCount() == 2);

    SUBCASE("unmounting what is not mounted is refused, not silent")
    {
        CHECK_FALSE(lFix.TheLevel.Unmount(MapId("Drop")));
        CHECK_FALSE(lFix.TheLevel.Unmount(MapId()));
    }
}

TEST_CASE("Level: the same map cannot be mounted twice")
{
    Fixture lFix("twice");
    lFix.Dir.Write("Maps/One.opaaxmap", MapText("33333333333333333333333333333333", "Only", "One"));

    CHECK(lFix.TheLevel.Mount(OpaaxString("Maps/One.opaaxmap")));
    CHECK_FALSE(lFix.TheLevel.Mount(OpaaxString("Maps/One.opaaxmap")));

    CHECK(lFix.TheWorld.GetEntityCount() == 1);
    CHECK(lFix.TheLevel.GetMountedMaps().size() == 1);

    // Mount does NOT touch the manifest — that is AddMap's job.
    CHECK(lFix.TheLevel.GetData().IsEmpty());
}

TEST_CASE("Level: AddMap and RemoveMap change the world AND the manifest")
{
    Fixture lFix("authoring");
    lFix.Dir.Write("Maps/Base.opaaxmap",  MapText("44444444444444444444444444444444", "InBase", "Base"));
    lFix.Dir.Write("Maps/Extra.opaaxmap", MapText("55555555555555555555555555555555", "InExtra", "Extra"));

    LevelData lData;
    lData.Name = OpaaxString("Authoring");
    lData.Maps.push_back(OpaaxString("Maps/Base.opaaxmap"));
    lFix.TheLevel.SetData(lData);
    REQUIRE(lFix.TheLevel.MountAll().IsValid());

    SUBCASE("AddMap mounts it and appends it")
    {
        CHECK(lFix.TheLevel.AddMap(OpaaxString("Maps/Extra.opaaxmap")));

        CHECK(lFix.TheLevel.GetData().MapCount() == 2);
        CHECK(lFix.TheWorld.GetEntityCount() == 2);
        CHECK(lFix.TheLevel.IsMounted(MapId("Extra")));

        // Twice is refused rather than duplicating the entry.
        CHECK_FALSE(lFix.TheLevel.AddMap(OpaaxString("Maps/Extra.opaaxmap")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 2);
    }

    SUBCASE("a map that cannot be read is NOT written into the manifest")
    {
        CHECK_FALSE(lFix.TheLevel.AddMap(OpaaxString("Maps/Nowhere.opaaxmap")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 1);
    }

    SUBCASE("RemoveMap refuses the persistent map")
    {
        // Removing it would silently re-point persistence at whatever ended up first — a bigger
        // decision than the caller asked for.
        CHECK_FALSE(lFix.TheLevel.RemoveMap(MapId("Base")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 1);
        CHECK(lFix.TheWorld.GetEntityCount() == 1);
    }

    SUBCASE("RemoveMap drops a non-persistent map from both")
    {
        REQUIRE(lFix.TheLevel.AddMap(OpaaxString("Maps/Extra.opaaxmap")));

        CHECK(lFix.TheLevel.RemoveMap(MapId("Extra")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 1);
        CHECK(lFix.TheWorld.GetEntityCount() == 1);
    }

    SUBCASE("SetPersistentMap moves the index, and the persistent index survives a removal ahead of it")
    {
        REQUIRE(lFix.TheLevel.AddMap(OpaaxString("Maps/Extra.opaaxmap")));

        CHECK(lFix.TheLevel.SetPersistentMap(MapId("Extra")));
        CHECK(lFix.TheLevel.GetData().PersistentMapIndex == 1);
        CHECK(lFix.TheLevel.GetPersistentMapId() == MapId("Extra"));

        // Base sits BEFORE it in the list, so removing Base must carry the index back with it —
        // left alone it would silently start naming a different map.
        CHECK(lFix.TheLevel.RemoveMap(MapId("Base")));
        CHECK(lFix.TheLevel.GetData().PersistentMapIndex == 0);
        CHECK(lFix.TheLevel.GetPersistentMapId() == MapId("Extra"));
    }
}

TEST_CASE("Level: adopting another level's mounts does NOT mount anything")
{
    // The PIE clone rule. A clone's entities arrive in the unfiltered snapshot (WM6), so the
    // Level has to copy what is mounted and put nothing in the world — mounting again would
    // re-read every map and CreateEntityWithGuid would refuse the lot (WM3).
    Fixture lSource("clone-src");
    lSource.Dir.Write("Maps/Only.opaaxmap", MapText("66666666666666666666666666666666", "InOnly", "Only"));

    LevelData lData;
    lData.Name = OpaaxString("Cloned");
    lData.Maps.push_back(OpaaxString("Maps/Only.opaaxmap"));
    lSource.TheLevel.SetData(lData);
    REQUIRE(lSource.TheLevel.MountAll().IsValid());

    World             lCloneWorld(OpaaxString("clone-dst"));
    ComponentRegistry lCloneComponents;
    ResourceManager   lCloneResources;
    TempPaths         lClonePaths(lSource.Dir.Root());
    Level             lCloneLevel(lCloneWorld, lCloneComponents, lClonePaths, lCloneResources);

    lCloneLevel.AdoptMountedFrom(lSource.TheLevel);

    CHECK(lCloneWorld.GetEntityCount() == 0);   // the snapshot's job, not the Level's
    CHECK(lCloneLevel.GetData().Name == OpaaxString("Cloned"));
    CHECK(lCloneLevel.IsMounted(MapId("Only")));
    CHECK(lCloneLevel.GetMountedMaps().size() == 1);
}
