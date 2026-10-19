#pragma once

#include "Core/Maths/Maths.h"

#include "Audio/AudioTypes.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // Spatialisation for a 2D game: a sound is quieter with distance from the listener and panned
    //   by how far it is to the left or right. No HRTF, no doppler.
    // =============================================================================
    namespace AudioSpatial
    {
        /** A max distance is kept above the min distance by at least this. */
        inline constexpr float MIN_DISTANCE_SPAN = 1.f;

        /**
         * Gain at InDistance: 1 within InMinDistance, 0 from InMaxDistance on.
         * Linear falls in a straight line; InverseSquare falls fast near the source then slowly, as
         *   (min / distance)^2, shifted so it still reaches 0 at the max distance.
         */
        inline float Attenuate(const EAudioAttenuation InCurve, const float InDistance, const float InMinDistance,
                               const float InMaxDistance) noexcept
        {
            const float lMin = Maths::Max(InMinDistance, 0.f);
            const float lMax = Maths::Max(InMaxDistance, lMin + MIN_DISTANCE_SPAN);

            if (InDistance <= lMin) { return 1.f; }
            if (InDistance >= lMax) { return 0.f; }

            switch (InCurve)
            {
            case EAudioAttenuation::InverseSquare:
            {
                // A min distance of 0 would make every distance silent: the curve starts at 1 unit.
                const float lRef   = Maths::Max(lMin, 1.f);
                const float lAtMax = (lRef / lMax) * (lRef / lMax);
                const float lHere  = (lRef / Maths::Max(InDistance, lRef)) * (lRef / Maths::Max(InDistance, lRef));
                return Maths::Clamp((lHere - lAtMax) / (1.f - lAtMax), 0.f, 1.f);
            }

            case EAudioAttenuation::Linear:
            default:
                return 1.f - (InDistance - lMin) / (lMax - lMin);
            }
        }

        /** -1 (left) to 1 (right): the sound's offset along X from the listener, over InMaxDistance. */
        inline float PanFromOffset(const float InOffsetX, const float InMaxDistance) noexcept
        {
            return Maths::Clamp(InOffsetX / Maths::Max(InMaxDistance, MIN_DISTANCE_SPAN), -1.f, 1.f);
        }

        /**
         * Left and right gains for a pan: the far side is lowered, the near side stays at full
         * gain, so a centred sound plays as recorded.
         */
        inline Vector2F BalanceGains(const float InPan) noexcept
        {
            const float lPan = Maths::Clamp(InPan, -1.f, 1.f);
            return Vector2F{ lPan > 0.f ? 1.f - lPan : 1.f, lPan < 0.f ? 1.f + lPan : 1.f };
        }
    }
}
