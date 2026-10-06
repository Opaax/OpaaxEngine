#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"

namespace Opaax::ECS
{
    // =============================================================================
    // TransformInterpolationComponent
    // =============================================================================
    /**
     * @struct TransformInterpolationComponent
     *
     * Runtime-only render state — NEVER serialized and NOT in the ComponentRegistry,
     * so it is invisible to scene save AND the PIE snapshot (which serializes the scene
     * before OnPlayBegin builds bodies). It exists only between play-begin and play-end.
     *
     * Holds the PREVIOUS fixed-step pose of a physics-driven entity; the CURRENT pose is
     * just the live TransformComponent. The fixed-step transform writers own it —
     * PhysicsSubsystem (dynamic bodies) and MoverSubsystem (movers) add it at OnPlayBegin
     * and snapshot the pre-step pose each FixedUpdate. WorldRenderSystem reads it and lerps
     * prev->current by the frame's interpolation alpha so motion stays smooth above the
     * 60 Hz fixed step. Display only — gameplay / ECS state stays the raw fixed-step value.
     */
    struct OPAAX_API TransformInterpolationComponent
    {
        Vector2F PrevPosition = { 0.f, 0.f };
        float    PrevRotation = 0.f;
    };
}
