#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct PrefabResource;   // NAMED, never completed — a path carries its type, not its header
}

namespace Sandbox
{
    // =============================================================================
    // GunComponent — the first component with a HARD reference (⑦-C P5b), and the case the
    //   design was argued from: a gun must not stall on its first shot, so the bullet prefab it
    //   spawns is `THardResourcePath` — resident the moment the gun's map mounts, released when it
    //   unmounts. The muzzle flash is `TResourcePath` — soft, loaded when first drawn — so the
    //   same type on the same component shows both policies side by side.
    //
    //   Declaring the field IS the whole opt-in: the registry finds it by type at registration,
    //   the Inspector gives it a typed drop target, and the Level holds it. No engine header
    //   names this component.
    // =============================================================================
    struct GunComponent
    {
        Opaax::THardResourcePath<Opaax::PrefabResource> Bullet;
        Opaax::TResourcePath<Opaax::PrefabResource>     MuzzleFlash;
        float                                            RateOfFire = 4.f;

        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GunComponent, Bullet, MuzzleFlash, RateOfFire)

        OPAAX_PROPERTIES(GunComponent,
                         OPAAX_PROP(Bullet),
                         OPAAX_PROP(MuzzleFlash),
                         OPAAX_PROP(RateOfFire))
    };
}
