#include "Core/Maths/Easing.h"

#include "Core/Maths/Maths.h"
#include "Core/Maths/MathsStatics.h"

namespace Opaax::Easing
{
    namespace
    {
        // Back's overshoot: about 10% past the end (Penner's constant).
        constexpr float BACK_OVERSHOOT       = 1.70158f;
        constexpr float BACK_OVERSHOOT_INOUT = BACK_OVERSHOOT * 1.525f;

        float BounceOut(const float InTime) noexcept
        {
            constexpr float lStrength = 7.5625f;
            constexpr float lSpan     = 2.75f;

            if (InTime < 1.f / lSpan)
            {
                return lStrength * InTime * InTime;
            }
            if (InTime < 2.f / lSpan)
            {
                const float lTime = InTime - 1.5f / lSpan;
                return lStrength * lTime * lTime + 0.75f;
            }
            if (InTime < 2.5f / lSpan)
            {
                const float lTime = InTime - 2.25f / lSpan;
                return lStrength * lTime * lTime + 0.9375f;
            }

            const float lTime = InTime - 2.625f / lSpan;
            return lStrength * lTime * lTime + 0.984375f;
        }
    }

    float Evaluate(const EEase InEase, const float InTime) noexcept
    {
        // Exact ends, whatever rounding the curves have.
        if (InTime <= 0.f)
        {
            return 0.f;
        }
        if (InTime >= 1.f)
        {
            return 1.f;
        }

        const float lT   = InTime;
        const float lInv = 1.f - lT;

        switch (InEase)
        {
        case EEase::Linear:
            return lT;

        case EEase::QuadIn:
            return lT * lT;
        case EEase::QuadOut:
            return 1.f - lInv * lInv;
        case EEase::QuadInOut:
            return lT < 0.5f ? 2.f * lT * lT : 1.f - 2.f * lInv * lInv;

        case EEase::CubicIn:
            return lT * lT * lT;
        case EEase::CubicOut:
            return 1.f - lInv * lInv * lInv;
        case EEase::CubicInOut:
            return lT < 0.5f ? 4.f * lT * lT * lT : 1.f - 4.f * lInv * lInv * lInv;

        case EEase::SineIn:
            return 1.f - Maths::Cos(lT * FHALF_PI);
        case EEase::SineOut:
            return Maths::Sin(lT * FHALF_PI);
        case EEase::SineInOut:
            return 0.5f * (1.f - Maths::Cos(lT * FPI));

        case EEase::BackIn:
            return lT * lT * ((BACK_OVERSHOOT + 1.f) * lT - BACK_OVERSHOOT);
        case EEase::BackOut:
            return 1.f - lInv * lInv * ((BACK_OVERSHOOT + 1.f) * lInv - BACK_OVERSHOOT);
        case EEase::BackInOut:
        {
            // Each half is BackIn on twice the time, the second mirrored.
            const float lHalf = lT < 0.5f ? 2.f * lT : 2.f * lInv;
            const float lBack = lHalf * lHalf * ((BACK_OVERSHOOT_INOUT + 1.f) * lHalf - BACK_OVERSHOOT_INOUT);
            return lT < 0.5f ? 0.5f * lBack : 1.f - 0.5f * lBack;
        }

        case EEase::ElasticOut:
            // A decaying wave: three swings past 1, the first the widest.
            return Maths::Pow(2.f, -10.f * lT) * Maths::Sin((10.f * lT - 0.75f) * (FTWO_PI / 3.f)) + 1.f;

        case EEase::BounceOut:
            return BounceOut(lT);
        }

        return lT;
    }
}
