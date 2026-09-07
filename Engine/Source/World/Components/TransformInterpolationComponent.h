#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "World/Components/TransformComponent.h"

namespace Opaax
{
    // =============================================================================
    // TransformInterpolationComponent — where an entity was at the END of the PREVIOUS fixed step.
    //
    //   RUNTIME ONLY, and deliberately NOT registered with the ComponentRegistry. That is what
    //   keeps it out of every map file, every snapshot and every PIE clone (**WM6** copies exactly
    //   what the registry knows) — a saved "previous pose" would be a lie the first time the map
    //   loaded, and it is not authored data in any sense.
    //
    //   WRITTEN BY THE FIXED-STEP WRITERS, read ONLY at render. Physics and the mover record the
    //   pose they are about to overwrite; the renderer blends it toward the current one by the
    //   frame's alpha. **Gameplay, queries and picking keep the RAW fixed-step value** — this
    //   exists so motion LOOKS right between steps, not so anything reasons about it.
    // =============================================================================
    struct TransformInterpolationComponent
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, matching TransformComponent. */
        float Rotation = 0.f;

        /**
         * False until a step has actually written it. The FIRST step has no previous pose, and
         * blending from a default-constructed one would fling the entity in from the origin.
         */
        bool bHasPrevious = false;
    };

    // =============================================================================
    // Display pose — the pure half, and therefore the testable one
    // =============================================================================
    /**
     * @struct DisplayPose
     * Where an entity should be DRAWN this frame. Never written back into the world.
     */
    struct DisplayPose
    {
        Vector2F Position    = { 0.f, 0.f };
        float    RotationDeg = 0.f;
    };

    /**
     * Blend the previous fixed-step pose toward the current one by InAlpha.
     *
     * @param InPrevious The previous pose, or nullptr when the entity has none — which is the
     *   common case (anything not driven by a fixed step) and answers the current pose exactly.
     * @param InAlpha [0..1] progress through the fixed step the renderer is between. Outside that
     *   range it is clamped rather than extrapolated: overshooting a pose the simulation has not
     *   produced is a different feature, and a worse default.
     *
     * Free and pure for `ToQuad`'s reason: the arithmetic is what can be wrong, and it needs no
     * world, no GL context and no clock to check.
     */
    inline DisplayPose ResolveDisplayPose(const TransformComponent& InCurrent,
                                          const TransformInterpolationComponent* InPrevious,
                                          const float InAlpha) noexcept
    {
        DisplayPose lPose;
        lPose.Position    = InCurrent.Position;
        lPose.RotationDeg = InCurrent.Rotation;

        if (InPrevious == nullptr || !InPrevious->bHasPrevious)
        {
            return lPose;
        }

        const float lAlpha = Maths::Clamp(InAlpha, 0.f, 1.f);

        lPose.Position = InPrevious->Position + (InCurrent.Position - InPrevious->Position) * lAlpha;

        // SHORTEST ARC. A naive lerp from 350 to 10 degrees travels backwards through 180 — a
        // visible spin the simulation never performed. Wrapping the DELTA into [-180, 180] first
        // takes the short way round, which is the way the body actually turned.
        float lDelta = InCurrent.Rotation - InPrevious->Rotation;

        while (lDelta > 180.f)  { lDelta -= 360.f; }
        while (lDelta < -180.f) { lDelta += 360.f; }

        lPose.RotationDeg = InPrevious->Rotation + lDelta * lAlpha;

        return lPose;
    }
}
