// Suite: ⑦-C P5b — the hard-ref acquire, headless.
//
// P5a gave a component the vocabulary (THardResourcePath<T>) and the registry the discovery
// (GetHardRefFields); this is the honouring. Two pure halves are pinned here: WHAT a set of
// entities must have resident (HardReferences::Collect over untyped data) and HOW a runtime type
// id becomes a typed load (ResourceFormatEntry::Acquire). The composed case — a mounted map holding
// them for as long as its entities live — is in LevelTests.
//
// Both halves are asserted from BOTH sides, the P5 spec's own rule: a hard field is collected and
// a soft one is NOT, or the test passes in a build where everything eager-loads.
#include <doctest.h>

#include <chrono>
#include <filesystem>

#include "Engine/Subsystems/Resources/ResourceFormatRegistry.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"   // the json bridge the map writer uses for a path
#include "Engine/Subsystems/Resources/ResourceTypeID.hpp"
#include "World/Components/ComponentRegistry.h"
#include "World/Prefab/PrefabFile.h"
#include "World/Prefab/PrefabResource.hpp"
#include "World/Serialization/HardReferences.h"
#include "World/Serialization/MapData.h"

using namespace Opaax;

namespace
{
    // The user's own case, one component: a gun naming the bullet it spawns (hard) and the flash it
    // draws (soft). PrefabResource is the real type, so the id the field carries is the real id.
    struct TurretGunComponent
    {
        THardResourcePath<PrefabResource> Bullet;
        TResourcePath<PrefabResource>     MuzzleFlash;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(TurretGunComponent, Bullet, MuzzleFlash)
        OPAAX_PROPERTIES(TurretGunComponent, OPAAX_PROP(Bullet), OPAAX_PROP(MuzzleFlash))
    };

    struct ScopedTempDir
    {
        std::filesystem::path Dir;

        explicit ScopedTempDir(const char* InTag)
        {
            const auto lTick = std::chrono::steady_clock::now().time_since_epoch().count();
            Dir = std::filesystem::temp_directory_path() / ("opaax_hardref_" + std::string(InTag) + "_" + std::to_string(lTick));
            std::filesystem::create_directories(Dir);
        }

        ~ScopedTempDir()
        {
            std::error_code lEc;
            std::filesystem::remove_all(Dir, lEc);
        }

        OpaaxString Sub(const char* InName) const { return OpaaxString((Dir / InName).string().c_str()); }
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

    // Exactly ONE — the bullet. The flash is the same resource type on the same component, and
    // the only thing separating the two is the field's declared policy.
    REQUIRE(lRefs.size() == 1);
    CHECK(lRefs[0].TypeId == ResourceTypeID::Get<PrefabResource>());
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

    // A component type this registry does not know contributes nothing — MP3's tolerance.
    MapData lUnknown = GunMap("Prefabs/Bullet.opaaxprefab", "");
    lUnknown.Entities[0].Components[0].TypeName = OpaaxStringID("NotRegistered");
    CHECK(HardReferences::Collect(lUnknown, lRegistry).empty());
}

TEST_CASE("HardReferences: a format entry's Acquire turns a type id into a typed, resident load")
{
    const ScopedTempDir lTemp("acquire");

    // A real prefab file — PrefabResource loads through PrefabFile, so no GL is involved.
    PrefabData lPrefab;
    EntityData lPiece;
    lPiece.Id   = Guid::New();
    lPiece.Name = OpaaxString("Bullet");
    lPrefab.Entities.emplace_back(Move(lPiece));
    const OpaaxString lFile = lTemp.Sub("Bullet.opaaxprefab");
    REQUIRE(PrefabFile::Save(lFile, lPrefab));

    ResourceFormatRegistry lFormats;
    REQUIRE(lFormats.Register<PrefabResource>(OPAAX_ID("Prefab")));

    ResourceManager lResources;
    REQUIRE(lResources.Startup());

    // By ID — the only thing a holder reading json ever has.
    const ResourceFormatEntry* const lEntry = lFormats.FindByTypeId(ResourceTypeID::Get<PrefabResource>());
    REQUIRE(lEntry != nullptr);
    REQUIRE(lEntry->Acquire != nullptr);

    {
        TUniquePtr<IResourceHold> lHold = lEntry->Acquire(lResources, lFile.CStr());
        REQUIRE(lHold != nullptr);
        CHECK(lHold->IsLoaded());
        CHECK(lResources.GetLoadedCount<PrefabResource>() == 1);   // resident while the hold lives

        // A FailFast type that cannot load answers a hold that is NOT loaded, never a placeholder.
        TUniquePtr<IResourceHold> lMissing = lEntry->Acquire(lResources, lTemp.Sub("Absent.opaaxprefab").CStr());
        REQUIRE(lMissing != nullptr);
        CHECK_FALSE(lMissing->IsLoaded());
    }

    // The hold was the claim: dropping it releases the resource.
    lResources.Update(0.0);   // the pump — an unreferenced slot is unloaded at the next pump
    CHECK(lResources.GetLoadedCount<PrefabResource>() == 0);

    lResources.FlushAll();
}
