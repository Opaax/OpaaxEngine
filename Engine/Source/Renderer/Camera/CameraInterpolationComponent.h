#pragma once

#include "Core/Maths/Maths.h"
#include "Renderer/Camera/CameraRhythm.h"

namespace Opaax
{
    // =============================================================================
    // CameraInterpolationComponent — the OrthoSize of the camera framing the world around its last
    //   fixed step. Runtime only, not registered (never saved). Written by the world around each
    //   step for the camera CameraManager records (CameraInterpolation); read by CameraManager to
    //   frame the zoom between the last two steps, as the pose is. Gameplay uses the real size.
    // =============================================================================
    struct CameraInterpolationComponent
    {
        /** The size at the start of the last step. */
        float PreviousOrthoSize = 0.f;

        /**
         * The size the last step left. A different size was set outside the steps (in Update) and
         * is framed as it is.
         */
        float SteppedOrthoSize = 0.f;

        /** False until a step was recorded. */
        bool bHasStep = false;

        /** The next frame is a cut (CameraManager::Cut): framed where it is, marked in traces. */
        bool bCut = false;

        /** How its view moves against the steps, counted at the start of each (CameraInterpolation). */
        CameraRhythm Rhythm;
    };

    // =============================================================================
    // Display size (pure function)
    // =============================================================================
    /**
     * The OrthoSize a camera frames with this frame: blended from its size at the start of the last
     * step toward the current one. Never written back.
     * @param InHistory The sizes recorded around the last step, or nullptr (gives the current size)
     * @param InAlpha Progress through the fixed step, clamped to [0..1]
     */
    inline float ResolveDisplayOrthoSize(const float InCurrent, const CameraInterpolationComponent* InHistory,
                                         const float InAlpha) noexcept
    {
        if (InHistory == nullptr || !InHistory->bHasStep)
        {
            return InCurrent;
        }

        // Set since the last step (in Update): framed as it is, not pulled back toward a size from
        // before the change.
        if (InCurrent != InHistory->SteppedOrthoSize)
        {
            return InCurrent;
        }

        const float lAlpha = Maths::Clamp(InAlpha, 0.f, 1.f);

        return InHistory->PreviousOrthoSize + (InCurrent - InHistory->PreviousOrthoSize) * lAlpha;
    }
}
