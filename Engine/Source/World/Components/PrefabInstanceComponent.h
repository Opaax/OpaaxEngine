#pragma once

#include <nlohmann/json.hpp>

#include "Core/GUID/Guid.h"
#include "Core/GUID/GuidJson.h"
#include "Engine/Subsystems/Resources/ResourcePath.h"
#include "Engine/Subsystems/Resources/ResourcePathJson.h"

namespace Opaax
{
    // Forward-declared: TResourcePath only needs the name.
    struct PrefabResource;

    // =============================================================================
    // PrefabInstanceComponent — marks an entity created from a prefab.
    //     Prefab       — which prefab (path).
    //     InstanceId   — which placement. Shared by the entities of one instance.
    //     TemplateGuid — which entity of the prefab (its id in the prefab file).
    //   Read-only in the Inspector: editing a guid would silently break the link.
    //   Saved in the map like any component.
    // =============================================================================
    struct PrefabInstanceComponent
    {
        /** Asset-relative ("Prefabs/Bullet.opaaxprefab"). Empty means the link is broken. */
        TResourcePath<PrefabResource> Prefab;

        /** Shared by every entity of one placement. Invalid means the link is broken. */
        Guid InstanceId;

        /** Which entity of the prefab this is (its id in the prefab file). */
        Guid TemplateGuid;

        /** Both parts must be set. */
        bool IsLinked() const noexcept
        {
            return !Prefab.IsEmpty() && InstanceId.IsValid() && TemplateGuid.IsValid();
        }

        // _WITH_DEFAULT: a missing key keeps its default, so maps saved before a new field still load.
        NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(PrefabInstanceComponent,
                                                    Prefab, InstanceId, TemplateGuid)
    };
}
