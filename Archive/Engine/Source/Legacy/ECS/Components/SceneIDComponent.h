#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::ECS
{
    // =============================================================================
    // SceneIDComponent
    // =============================================================================
    /**
     * @struct SceneIDComponent
     *
     * Runtime-only tag identifying which Scene contributed an entity.
     *
     * SceneID == 0 marks the entity as persistent — owned by no Scene, survives all
     * Push/Pop cycles. Scene-owned entities are stamped by World::PushScene with the
     * Scene's runtime SceneID.
     *
     * Not serialized to disk: SceneID values are monotonic per-process and have no
     * meaning across save/load. On-disk scene identity is the SceneAsset path /
     * AssetID — see SceneSerializer.
     */
    struct OPAAX_API SceneIDComponent
    {
        // =============================================================================
        // Members
        // =============================================================================
        Uint32 SceneID = 0;

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
        SceneIDComponent() = default;
        explicit SceneIDComponent(Uint32 InSceneID) noexcept : SceneID(InSceneID) {}
    };
}
