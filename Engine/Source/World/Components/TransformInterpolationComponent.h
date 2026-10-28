#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "World/Components/TransformComponent.h"

namespace Opaax
{
    // =============================================================================
    // TransformInterpolationComponent — the pose at the end of the previous fixed step, and the one
    //   the last step left. Runtime only, not registered (never saved). Written by physics and the
    //   mover, and by the world around each step for the entities that ask for it
    //   (TransformInterpolation); read by the renderer and the camera to smooth motion between steps.
    //   Gameplay always uses the real pose.
    // =============================================================================
    struct TransformInterpolationComponent
    {
        Vector2F Position = { 0.f, 0.f };

        /** Degrees. */
        float Rotation = 0.f;

        /**
         * False until a step has written it (the first step has no previous pose).
         */
        bool bHasPrevious = false;

        /**
         * The world records this entity's pose at the start and the end of every fixed step (an
         * entity gameplay moves in a step, such as a camera). See TransformInterpolation.
         */
        bool bRecordSteps = false;

        /**
         * The local pose the last step left (recorded by the world, physics and the mover). A
         * different pose was set outside the steps (in Update) and is drawn as it is.
         */
        Vector2F SteppedPosition = { 0.f, 0.f };
        float    SteppedRotation = 0.f;
        bool     bHasStepped     = false;
    };

    // =============================================================================
    // Display pose (pure functions)
    // =============================================================================
    /**
     * Where an entity is drawn this frame, in world space. Never written back.
     */
    struct DisplayPose
    {
        Vector2F Position    = { 0.f, 0.f };
        float    RotationDeg = 0.f;
        Vector2F Scale       = { 1.f, 1.f };   // not interpolated
    };

    /**
     * Blends the previous fixed-step pose toward the current one.
     * @param InPrevious The previous pose, or nullptr (gives the current pose)
     * @param InAlpha Progress through the fixed step, clamped to [0..1]
     */
    inline DisplayPose ResolveDisplayPose(const TransformComponent& InCurrent,
                                          const TransformInterpolationComponent* InPrevious,
                                          const float InAlpha) noexcept
    {
        DisplayPose lPose;
        lPose.Position    = InCurrent.Position;
        lPose.RotationDeg = InCurrent.Rotation;
        lPose.Scale       = InCurrent.Scale;

        if (InPrevious == nullptr || !InPrevious->bHasPrevious)
        {
            return lPose;
        }

        // Moved since the last step (in Update): drawn where it was put, not pulled back toward
        // a pose from before the move.
        if (InPrevious->bHasStepped
            && (InCurrent.Position != InPrevious->SteppedPosition || InCurrent.Rotation != InPrevious->SteppedRotation))
        {
            return lPose;
        }

        const float lAlpha = Maths::Clamp(InAlpha, 0.f, 1.f);

        lPose.Position = InPrevious->Position + (InCurrent.Position - InPrevious->Position) * lAlpha;

        // Shortest arc: 350 -> 10 degrees goes through 0, not 180.
        float lDelta = InCurrent.Rotation - InPrevious->Rotation;

        while (lDelta > 180.f)  { lDelta -= 360.f; }
        while (lDelta < -180.f) { lDelta += 360.f; }

        lPose.RotationDeg = InPrevious->Rotation + lDelta * lAlpha;

        return lPose;
    }
}
