#pragma once

#include <nlohmann/json.hpp>

#include "Core/Reflection/OpaaxProperty.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    struct PrefabResource;   // declared only: a path carries its type, not its header
}

namespace Sandbox
{
    // =============================================================================
    // GunComponent — a component with both kinds of resource reference. The bullet prefab is a
    //   THardResourcePath (loaded when the gun's map loads, so the first shot does not stall); the
    //   muzzle flash is a soft TResourcePath (loaded when first drawn). Declaring the fields is all it
    //   takes: the Inspector gives them typed drop targets.
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
