#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "ECS/OpaaxEntity.hpp"

namespace Opaax
{
    class World;
}

namespace Opaax::ECS::Hierarchy
{
    /**
     * @struct WorldTransform
     * Result of compounding an entity's local TransformComponent with all its ancestors.
     * Pure POD, computed on demand by GetWorldTransform — no caching for now.
     */
    struct WorldTransform
    {
        Vector2F Position = { 0.f, 0.f };
        Vector2F Scale    = { 1.f, 1.f };
        float    Rotation = 0.f;
        float    ZOrder   = 0.f;
    };

    /**
     * Set InChild's parent to InNewParent. Rejects (and returns false) if:
     *  - InChild or InNewParent is invalid,
     *  - InNewParent is the same as InChild,
     *  - InNewParent is reachable from InChild via parent chain (would create a cycle).
     * Pass ENTITY_NONE as InNewParent to detach (equivalent to ClearParent).
     */
    OPAAX_API bool SetParent(World& InWorld, EntityID InChild, EntityID InNewParent);

    /** Detach InChild from any parent. No-op if it had none. */
    OPAAX_API void ClearParent(World& InWorld, EntityID InChild);

    /** True if InAncestor appears anywhere on InEntity's parent chain (excluding InEntity itself). */
    OPAAX_API bool IsDescendantOf(const World& InWorld, EntityID InEntity, EntityID InAncestor);

    /**
     * Walks the parent chain and compounds local transforms.
     * - Position: parent's world rotation applied to local position * parent scale, then translated.
     * - Scale:    component-wise multiply.
     * - Rotation: simple sum (radians).
     * - ZOrder:   simple sum.
     * Entities without a TransformComponent contribute identity. Cycles are guarded by a depth limit.
     */
    OPAAX_API WorldTransform GetWorldTransform(const World& InWorld, EntityID InEntity);
}
