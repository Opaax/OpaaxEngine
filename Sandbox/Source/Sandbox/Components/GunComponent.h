#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Registries/AutoRegistration.h"
#include "Resources/Asset/AssetRef.h"
#include "World/Prefab/PrefabAsset.h"   // complete: the component is registered here

namespace Sandbox
{
    // =============================================================================
    // GunComponent — a component with both kinds of asset reference. The bullet prefab is a
    //   THardAssetRef (loaded when the gun's map loads, so the first shot does not stall); the muzzle
    //   flash is a soft TAssetRef (loaded when first spawned). Declaring the fields is all it takes:
    //   the Inspector gives them drop targets that take prefabs only.
    // =============================================================================
    struct GunComponent
    {
        Opaax::THardAssetRef<Opaax::PrefabAsset> Bullet;
        Opaax::TAssetRef<Opaax::PrefabAsset>     MuzzleFlash;
        float                                     RateOfFire = 4.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GunComponent, Bullet, MuzzleFlash, RateOfFire)

        OPAAX_PROPERTIES(GunComponent,
                         OPAAX_PROP(Bullet),
                         OPAAX_PROP(MuzzleFlash),
                         OPAAX_PROP(RateOfFire))
    };

    // A hard reference: a level loading a map with a gun keeps its bullet prefab loaded.
    OPAAX_REGISTER_COMPONENT(GunComponent);
}
