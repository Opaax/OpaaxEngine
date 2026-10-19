#pragma once

#include "Core/Maths/Maths.h"

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // AudioFader — a gain that moves linearly to a target over a number of frames. Voices and
    //   buses each have one: fade-in, fade-out, FadeTo and a stop without a click are all fades.
    // =============================================================================
    struct AudioFader
    {
        float  Current         = 1.f;
        float  Target          = 1.f;
        Uint32 RemainingFrames = 0;

        /** Starts moving to InTarget, reached in InFrames (0: at once). */
        void FadeTo(const float InTarget, const Uint32 InFrames) noexcept
        {
            Target          = InTarget;
            RemainingFrames = InFrames;
            if (InFrames == 0)
            {
                Current = InTarget;
            }
        }

        /** Jumps to InValue, with nothing left to fade. */
        void Set(const float InValue) noexcept
        {
            Current         = InValue;
            Target          = InValue;
            RemainingFrames = 0;
        }

        /** Advances by InFrames. */
        void Tick(const Uint32 InFrames) noexcept
        {
            if (RemainingFrames == 0)
            {
                Current = Target;
                return;
            }

            const Uint32 lFrames = Maths::Min(InFrames, RemainingFrames);
            Current += (Target - Current) * (static_cast<float>(lFrames) / static_cast<float>(RemainingFrames));
            RemainingFrames -= lFrames;

            if (RemainingFrames == 0)
            {
                Current = Target;
            }
        }

        bool IsFading() const noexcept { return RemainingFrames > 0; }

        /** Done fading out: nothing will be heard any more. */
        bool IsSilent() const noexcept { return RemainingFrames == 0 && Current <= 0.f; }
    };
}
