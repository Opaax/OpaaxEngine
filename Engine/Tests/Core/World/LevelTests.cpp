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
#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"   // P5b — a gun's fields, written as the map writer would
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/Level.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/LevelFile.h"
#include "World/Serialization/MapFile.h"
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

    // The five references a Level is built from, bundled so a case reads as its own story.
    struct Fixture
    {
        ScopedTempDir          Dir;
        World                  TheWorld;
        ComponentRegistry      Components;
        ResourceManager        Resources;
        ResourceFormatRegistry Formats;
        TempPaths              Paths;
        Level                  TheLevel;

        explicit Fixture(const char* InTag)
            : Dir(InTag)
            , TheWorld(OpaaxString(InTag))
            , Paths(Dir.Root())
            , TheLevel(TheWorld, Components, Paths, Resources, Formats)
        {
        }
    };

    // P5b — the user's own case: a gun naming the bullet it spawns (hard) and the flash it draws
    // (soft). Real PrefabResource on both, so the only thing separating them is the declared policy.
    struct GunComponent
    {
        THardResourcePath<PrefabResource> Bullet;
        TResourcePath<PrefabResource>     MuzzleFlash;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GunComponent, Bullet, MuzzleFlash)
        OPAAX_PROPERTIES(GunComponent, OPAAX_PROP(Bullet), OPAAX_PROP(MuzzleFlash))
    };

    // A one-entity map carrying a gun, written by the real writer, plus the prefab it names.
    void WriteGunMap(const Fixture& InFix, const char* InBulletField, const char* InFlashField)
    {
        PrefabData lBullet;
        EntityData lPiece;
        lPiece.Id   = Guid::New();
        lPiece.Name = OpaaxString("Bullet");
        lBullet.Entities.emplace_back(Move(lPiece));
        std::error_code lError;
        fs::create_directories(InFix.Dir.Root() / "Prefabs", lError);
        REQUIRE(PrefabFile::Save(InFix.Paths.AssetToAbsolute(OpaaxString("Prefabs/Bullet.opaaxprefab")), lBullet));

        GunComponent lGun;
        lGun.Bullet.Path      = OpaaxString(InBulletField);
        lGun.MuzzleFlash.Path = OpaaxString(InFlashField);

        EntityData lTurret;
        lTurret.Id       = Guid::New();
        lTurret.Name     = OpaaxString("Turret");
        lTurret.OwnerMap = MapId("Guns");
        lTurret.Components.emplace_back(OpaaxStringID("Gun"), nlohmann::json(lGun));

        MapData lMap;
        lMap.Id = MapId("Guns");
        lMap.Entities.emplace_back(Move(lTurret));
        REQUIRE(MapFile::Save(InFix.Paths.AssetToAbsolute(OpaaxString("Maps/Guns.opaaxmap")), lMap));
    }
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

TEST_CASE("Level: a missing manifest entry can be REMOVED, which is the only repair there is")
{
    // The entry that never mounted has no MapId — an id comes from the file's entities (MP10) and
    // there is no file — so RemoveMap cannot name it and the Hierarchy, which lists MOUNTED maps,
    // had no row to hang a menu on. Before this verb the level warned on every boot forever and the
    // only fix was hand-editing the .opaaxlevel.
    Fixture lFix("removemissing");
    lFix.Dir.Write("Maps/Here.opaaxmap", MapText("11111111111111111111111111111111", "Here", "Here"));

    LevelData lData;
    lData.Name = OpaaxString("Broken");
    lData.Maps.push_back(OpaaxString("Maps/Here.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/Gone.opaaxmap"));
    lFix.TheLevel.SetData(lData);

    REQUIRE_FALSE(lFix.TheLevel.MountAll().IsValid());
    REQUIRE(lFix.TheLevel.GetData().MapCount() == 2);

    SUBCASE("the missing entry goes, and the level opens cleanly afterwards")
    {
        CHECK(lFix.TheLevel.RemoveMissingMap(OpaaxString("Maps/Gone.opaaxmap")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 1);

        // THE POINT OF THE WHOLE VERB: what was a permanently-warning level is now a valid one.
        Fixture lReopened("removemissing2");
        lReopened.Dir.Write("Maps/Here.opaaxmap", MapText("11111111111111111111111111111111", "Here", "Here"));
        lReopened.TheLevel.SetData(lFix.TheLevel.GetData());

        CHECK(lReopened.TheLevel.MountAll().IsValid());
    }

    SUBCASE("a MOUNTED path is refused — its entities must go through RemoveMap")
    {
        CHECK_FALSE(lFix.TheLevel.RemoveMissingMap(OpaaxString("Maps/Here.opaaxmap")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 2);
        CHECK(lFix.TheLevel.IsMounted(MapId("Here")));
    }

    SUBCASE("a path that is not in the manifest at all is refused")
    {
        CHECK_FALSE(lFix.TheLevel.RemoveMissingMap(OpaaxString("Maps/NeverThere.opaaxmap")));
        CHECK(lFix.TheLevel.GetData().MapCount() == 2);
    }
}

TEST_CASE("Level: removing a missing entry keeps PersistentMapIndex on the same map")
{
    // The index is a POSITION, so dropping anything ahead of it silently re-points persistence at
    // the next map along — the bug this shares with RemoveMap, which is why both go through one
    // erase helper rather than two copies of the fix-up.
    Fixture lFix("missingpersist");
    lFix.Dir.Write("Maps/Real.opaaxmap", MapText("22222222222222222222222222222222", "InReal", "Real"));

    LevelData lData;
    lData.Name = OpaaxString("Shifted");
    lData.Maps.push_back(OpaaxString("Maps/Gone.opaaxmap"));   // index 0 — missing, and FIRST
    lData.Maps.push_back(OpaaxString("Maps/Real.opaaxmap"));   // index 1 — the persistent one
    lData.PersistentMapIndex = 1;
    lFix.TheLevel.SetData(lData);

    REQUIRE_FALSE(lFix.TheLevel.MountAll().IsValid());

    CHECK(lFix.TheLevel.RemoveMissingMap(OpaaxString("Maps/Gone.opaaxmap")));

    // Still Real, now at index 0. Left alone the index would name nothing at all.
    CHECK(lFix.TheLevel.GetData().MapCount() == 1);
    CHECK(lFix.TheLevel.GetData().PersistentMap() == OpaaxString("Maps/Real.opaaxmap"));
}

TEST_CASE("Level: the PERSISTENT map is refused even when it is the missing one")
{
    // Dropping it would re-point persistence at whatever ended up first — a bigger decision than
    // "remove this map", and the last thing an author repairing a broken level wants unasked.
    Fixture lFix("missingpersistrefuse");
    lFix.Dir.Write("Maps/Real.opaaxmap", MapText("33333333333333333333333333333333", "InReal", "Real"));

    LevelData lData;
    lData.Name = OpaaxString("PersistGone");
    lData.Maps.push_back(OpaaxString("Maps/Gone.opaaxmap"));
    lData.Maps.push_back(OpaaxString("Maps/Real.opaaxmap"));
    lData.PersistentMapIndex = 0;   // the MISSING one is the backdrop
    lFix.TheLevel.SetData(lData);

    REQUIRE_FALSE(lFix.TheLevel.MountAll().IsValid());

    CHECK_FALSE(lFix.TheLevel.RemoveMissingMap(OpaaxString("Maps/Gone.opaaxmap")));
    CHECK(lFix.TheLevel.GetData().MapCount() == 2);
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
    ComponentRegistry      lCloneComponents;
    ResourceManager        lCloneResources;
    ResourceFormatRegistry lCloneFormats;
    TempPaths              lClonePaths(lSource.Dir.Root());
    Level                  lCloneLevel(lCloneWorld, lCloneComponents, lClonePaths, lCloneResources, lCloneFormats);

    lCloneLevel.AdoptMountedFrom(lSource.TheLevel);

    CHECK(lCloneWorld.GetEntityCount() == 0);   // the snapshot's job, not the Level's
    CHECK(lCloneLevel.GetData().Name == OpaaxString("Cloned"));
    CHECK(lCloneLevel.IsMounted(MapId("Only")));
    CHECK(lCloneLevel.GetMountedMaps().size() == 1);
}

// =============================================================================
// ⑦-C P5b — the composed gate: a mounted map HOLDS what its entities must have resident
// =============================================================================

TEST_CASE("Level: a HARD reference is resident the moment its map mounts, and released when it unmounts")
{
    Fixture lFix("hardref");
    REQUIRE(lFix.Resources.Startup());
    REQUIRE(lFix.Components.Register<GunComponent>("Gun"));
    REQUIRE(lFix.Formats.Register<PrefabResource>(OPAAX_ID("Prefab")));

    // The gun names the bullet through its HARD field.
    WriteGunMap(lFix, "Prefabs/Bullet.opaaxprefab", "");

    LevelData lData;
    lData.Name = OpaaxString("Guns");
    lData.Maps.push_back(OpaaxString("Maps/Guns.opaaxmap"));
    lFix.TheLevel.SetData(lData);

    CHECK(lFix.Resources.GetLoadedCount<PrefabResource>() == 0);   // nothing has asked for it yet

    const Level::MountResult lResult = lFix.TheLevel.MountAll();
    REQUIRE(lResult.MapsMounted == 1);
    CHECK(lResult.EntitiesCreated == 1);

    // IMMEDIATELY resident — nobody resolved it, the mount did — and held by the record.
    CHECK(lFix.Resources.GetLoadedCount<PrefabResource>() == 1);
    REQUIRE(lFix.TheLevel.GetMountedMaps().size() == 1);
    CHECK(lFix.TheLevel.GetMountedMaps()[0].HardRefs.size() == 1);

    // Still resident across a pump: the hold is a live claim, not a grace window.
    lFix.Resources.Update(0.0);
    CHECK(lFix.Resources.GetLoadedCount<PrefabResource>() == 1);

    // Unmount drops the record, the record drops the hold, the pump collects.
    REQUIRE(lFix.TheLevel.Unmount(MapId("Guns")));
    lFix.Resources.Update(0.0);
    CHECK(lFix.Resources.GetLoadedCount<PrefabResource>() == 0);

    lFix.Resources.FlushAll();
}

TEST_CASE("Level: the SAME map with the bullet on a SOFT field holds nothing")
{
    // The half that fails in a build where everything eager-loads: same component, same type,
    // same file, one field over — and nothing is resident, because nothing asked.
    Fixture lFix("softref");
    REQUIRE(lFix.Resources.Startup());
    REQUIRE(lFix.Components.Register<GunComponent>("Gun"));
    REQUIRE(lFix.Formats.Register<PrefabResource>(OPAAX_ID("Prefab")));

    WriteGunMap(lFix, "", "Prefabs/Bullet.opaaxprefab");

    LevelData lData;
    lData.Name = OpaaxString("Guns");
    lData.Maps.push_back(OpaaxString("Maps/Guns.opaaxmap"));
    lFix.TheLevel.SetData(lData);

    REQUIRE(lFix.TheLevel.MountAll().MapsMounted == 1);

    CHECK(lFix.Resources.GetLoadedCount<PrefabResource>() == 0);
    REQUIRE(lFix.TheLevel.GetMountedMaps().size() == 1);
    CHECK(lFix.TheLevel.GetMountedMaps()[0].HardRefs.empty());

    lFix.Resources.FlushAll();
}
