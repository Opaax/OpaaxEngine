// Suite: hard references (THardAssetRef). What a set of entities must have loaded
// (HardReferences::Collect), and how a type id becomes a typed load (ResourceFormatEntry::Acquire).
// Hard fields are collected, soft ones are not. The mounted-map case is in LevelTests.
#include <doctest.h>

#include "../TestTempDir.h"
#include "Object/ObjectClassRegistry.h"
#include "Resources/Asset/Asset.h"
#include "Resources/Asset/AssetFile.h"
#include "Resources/Asset/AssetRef.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceFormatRegistry.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourceTypeID.hpp"
#include "World/Components/ComponentRegistry.h"
#include "World/Prefab/PrefabAsset.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Serialization/HardReferences.h"
#include "World/Serialization/MapData.h"

using namespace Opaax;

namespace
{
    // The user's own case, one component: a gun naming the bullet it spawns (hard) and the flash it
    // spawns (soft). PrefabAsset is the real class, so what the field asks for is what a level loads.
    struct TurretGunComponent
    {
        THardAssetRef<PrefabAsset> Bullet;
        TAssetRef<PrefabAsset>     MuzzleFlash;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TurretGunComponent, Bullet, MuzzleFlash)
        OPAAX_PROPERTIES(TurretGunComponent, OPAAX_PROP(Bullet), OPAAX_PROP(MuzzleFlash))
    };


    // An asset a turret holds hard (its shell) or soft (the scorch it leaves).
    class TurretShell final : public Asset
    {
        OPAAX_OBJECT(TurretShell, Asset)

    public:
        Int32 Damage = 1;
        OPAAX_PROPERTIES(TurretShell, OPAAX_PROP(Damage))
    };

    struct TurretAmmoComponent
    {
        THardAssetRef<TurretShell> Shell;
        TAssetRef<TurretShell>     Scorch;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TurretAmmoComponent, Shell, Scorch)
        OPAAX_PROPERTIES(TurretAmmoComponent, OPAAX_PROP(Shell), OPAAX_PROP(Scorch))
    };

    // One entity carrying a gun with the given field values, as the json bridge would write it.
    MapData GunMap(const char* InBullet, const char* InMuzzleFlash)
    {
        TurretGunComponent lGun;
        lGun.Bullet.Path      = OpaaxString(InBullet);
        lGun.MuzzleFlash.Path = OpaaxString(InMuzzleFlash);

        EntityData lEntity;
        lEntity.Id   = Guid::New();
        lEntity.Name = OpaaxString("Turret");
        lEntity.Components.emplace_back(OpaaxStringID("TurretGun"), nlohmann::json(lGun));

        MapData lData;
        lData.Entities.emplace_back(Move(lEntity));
        return lData;
    }
}

TEST_CASE("HardReferences: Collect names the HARD field and not the soft one beside it")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<TurretGunComponent>("TurretGun"));

    const MapData lData = GunMap("Prefabs/Bullet.opaaxprefab", "Prefabs/Flash.opaaxprefab");

    const TDynArray<HardReference> lRefs = HardReferences::Collect(lData, lRegistry);

    // Exactly ONE — the bullet. The flash is the same class on the same component, and the only thing
    // separating the two is the field's declared policy.
    REQUIRE(lRefs.size() == 1);
    CHECK(lRefs[0].TypeId == ResourceTypeID::Get<AssetResource>());
    CHECK(lRefs[0].Path   == OpaaxString("Prefabs/Bullet.opaaxprefab"));
}

TEST_CASE("HardReferences: an empty hard field is a state, not a reference; and duplicates fold")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<TurretGunComponent>("TurretGun"));

    // "No bullet yet" — nothing to hold, nothing to warn about.
    CHECK(HardReferences::Collect(GunMap("", "Prefabs/Flash.opaaxprefab"), lRegistry).empty());

    // Two guns naming one bullet hold it ONCE — the manager dedups anyway, but the holder should
    // not carry two claims for one intent.
    MapData lTwo = GunMap("Prefabs/Bullet.opaaxprefab", "");
    lTwo.Entities.emplace_back(GunMap("Prefabs/Bullet.opaaxprefab", "").Entities[0]);
    CHECK(HardReferences::Collect(lTwo, lRegistry).size() == 1);

    // A component type this registry does not know contributes nothing.
    MapData lUnknown = GunMap("Prefabs/Bullet.opaaxprefab", "");
    lUnknown.Entities[0].Components[0].TypeName = OpaaxStringID("NotRegistered");
    CHECK(HardReferences::Collect(lUnknown, lRegistry).empty());
}

TEST_CASE("HardReferences: a format entry's Acquire turns a type id into a typed, resident load")
{
    const TestTempDir lTemp("acquire");

    // A real prefab file — a PrefabAsset reads it through PrefabFile, so no GL is involved.
    PrefabData lPrefab;
    EntityData lPiece;
    lPiece.Id   = Guid::New();
    lPiece.Name = OpaaxString("Bullet");
    lPrefab.Entities.emplace_back(Move(lPiece));
    const OpaaxString lFile = lTemp.Sub("Bullet.opaaxprefab");
    REQUIRE(PrefabFile::Save(lFile, lPrefab));

    ResourceFormatRegistry lFormats;
    REQUIRE(lFormats.Register<AssetResource>(OPAAX_ID("Asset")));

    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<PrefabAsset>());

    ResourceManager lResources;
    lResources.SetObjectClasses(&lClasses);
    REQUIRE(lResources.Startup());

    // By ID — the only thing a holder reading json ever has.
    const ResourceFormatEntry* const lEntry = lFormats.FindByTypeId(ResourceTypeID::Get<AssetResource>());
    REQUIRE(lEntry != nullptr);
    REQUIRE(lEntry->Acquire != nullptr);

    {
        TUniquePtr<IResourceHold> lHold = lEntry->Acquire(lResources, lFile.CStr());
        REQUIRE(lHold != nullptr);
        CHECK(lHold->IsLoaded());
        CHECK(lResources.GetLoadedCount<AssetResource>() == 1);   // resident while the hold lives

        // A file that cannot load answers a hold that is NOT loaded, though a placeholder stands in.
        TUniquePtr<IResourceHold> lMissing = lEntry->Acquire(lResources, lTemp.Sub("Absent.opaaxprefab").CStr());
        REQUIRE(lMissing != nullptr);
        CHECK_FALSE(lMissing->IsLoaded());
    }

    // The hold was the claim: dropping it releases the resource.
    lResources.Update(0.0);   // the pump — an unreferenced slot is unloaded at the next pump
    CHECK(lResources.GetLoadedCount<AssetResource>() == 0);

    lResources.FlushAll();
}

TEST_CASE("HardReferences: a hard asset reference is collected as an asset file; the soft one beside it is not")
{
    ComponentRegistry lRegistry;
    REQUIRE(lRegistry.Register<TurretAmmoComponent>("TurretAmmo"));

    TurretAmmoComponent lAmmo;
    lAmmo.Shell.Path  = OpaaxString("Data/Shell.opaaxasset");
    lAmmo.Scorch.Path = OpaaxString("Data/Scorch.opaaxasset");

    EntityData lEntity;
    lEntity.Id   = Guid::New();
    lEntity.Name = OpaaxString("Turret");
    lEntity.Components.emplace_back(OpaaxStringID("TurretAmmo"), nlohmann::json(lAmmo));

    MapData lData;
    lData.Entities.emplace_back(Move(lEntity));

    // Every asset file loads as an AssetResource, whatever its class.
    const TDynArray<HardReference> lRefs = HardReferences::Collect(lData, lRegistry);
    REQUIRE(lRefs.size() == 1);
    CHECK(lRefs[0].TypeId == ResourceTypeID::Get<AssetResource>());
    CHECK(lRefs[0].Path   == OpaaxString("Data/Shell.opaaxasset"));
}

TEST_CASE("HardReferences: the asset entry's Acquire keeps a hard asset loaded while the hold lives")
{
    const TestTempDir lTemp("acquire-asset");

    ObjectClassRegistry lClasses;
    REQUIRE(lClasses.Register<TurretShell>());

    TurretShell lShell;
    lShell.Damage = 9;
    const OpaaxString lFile = lTemp.Sub("Shell.opaaxasset");
    REQUIRE(AssetFile::Save(lFile, lShell));

    ResourceFormatRegistry lFormats;
    REQUIRE(lFormats.Register<AssetResource>(OPAAX_ID("Asset")));

    ResourceManager lResources;
    lResources.SetObjectClasses(&lClasses);
    REQUIRE(lResources.Startup());

    const ResourceFormatEntry* const lEntry = lFormats.FindByTypeId(ResourceTypeID::Get<AssetResource>());
    REQUIRE(lEntry != nullptr);

    {
        TUniquePtr<IResourceHold> lHold = lEntry->Acquire(lResources, lFile.CStr());
        REQUIRE(lHold != nullptr);
        CHECK(lHold->IsLoaded());
        CHECK(lResources.GetLoadedCount<AssetResource>() == 1);
    }

    lResources.Update(0.0);
    CHECK(lResources.GetLoadedCount<AssetResource>() == 0);

    lResources.FlushAll();
}
