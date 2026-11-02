#include "Renderer/Camera/CameraShake.h"

#include "Core/Maths/Maths.h"
#include "Core/Maths/MathsStatics.h"
#include "Core/Templates/TypeTraits.h"
#include "Renderer/Camera/CameraBlend.h"

namespace Opaax
{
    namespace
    {
        /** A seed's phase for one axis, 0 to 1: the golden ratio's steps spread neighbouring seeds apart. */
        double PhaseOf(const Uint32 InSeed, const Uint32 InAxis) noexcept
        {
            const double lStep = static_cast<double>(InSeed) * 3.0 + static_cast<double>(InAxis);
            const double lTurn = lStep * 0.6180339887498949;
            return lTurn - Maths::Floor(lTurn);
        }

        /** Each axis its own noise: the seed spread, so neighbouring seeds share nothing. */
        Uint32 AxisSeed(const Uint32 InSeed, const Uint32 InAxis) noexcept
        {
            return InSeed * 3u + InAxis;
        }

        float Ease(const double InTime) noexcept
        {
            return CameraBlendWeight(ECameraBlend::EaseInOut, static_cast<float>(InTime));
        }
    }

    double CameraShake::EndTime(const TOptional<double> InStoppedAt) const noexcept
    {
        double lEnd = Duration > 0.f ? static_cast<double>(Duration) : TNumericLimits<double>::infinity();
        if (InStoppedAt.has_value())
        {
            lEnd = Maths::Min(lEnd, *InStoppedAt + static_cast<double>(BlendOut));
        }

        return lEnd;
    }

    float CameraShake::Envelope(const double InElapsed, const TOptional<double> InStoppedAt) const noexcept
    {
        if (InElapsed < 0.0 || InElapsed >= EndTime(InStoppedAt))
        {
            return 0.f;
        }

        float lStrength = BlendIn > 0.f ? Ease(InElapsed / BlendIn) : 1.f;
        if (BlendOut > 0.f)
        {
            if (Duration > 0.f)
            {
                lStrength = Maths::Min(lStrength, Ease((Duration - InElapsed) / BlendOut));
            }

            if (InStoppedAt.has_value())
            {
                lStrength = Maths::Min(lStrength, Ease(1.0 - (InElapsed - *InStoppedAt) / BlendOut));
            }
        }

        return lStrength;
    }

    CameraShakeOffset CameraShake::Oscillate(const Uint32 InSeed, const double InElapsed) const noexcept
    {
        const double lCycles = InElapsed * static_cast<double>(Frequency);

        // Each axis a quarter cycle apart: noise is 0 at every whole cycle, and the axes would stop together.
        const auto lAxis = [&](const Uint32 InAxis) -> float
        {
            const double lAt = lCycles + 0.25 * static_cast<double>(InAxis);
            return Pattern == ECameraShakePattern::Wave
                ? static_cast<float>(Maths::Sin(DTWO_PI * (lAt + PhaseOf(InSeed, InAxis))))
                : Maths::Noise1D(AxisSeed(InSeed, InAxis), lAt);
        };

        CameraShakeOffset lOffset;
        lOffset.Position = Vector2F(lAxis(0), lAxis(1)) * Amplitude;
        lOffset.Rotation = lAxis(2) * RollAmplitude;
        return lOffset;
    }

    CameraShakeOffset CameraShake::Sample(const Uint32 InSeed, const double InElapsed,
                                          const TOptional<double> InStoppedAt) const noexcept
    {
        const float lStrength = Envelope(InElapsed, InStoppedAt);
        if (lStrength <= 0.f)
        {
            return {};
        }

        CameraShakeOffset lOffset = Oscillate(InSeed, InElapsed);
        lOffset.Position *= lStrength;
        lOffset.Rotation *= lStrength;
        return lOffset;
    }

    void CameraShake::PostEditChange()
    {
        Amplitude     = Maths::Max(Amplitude, 0.f);
        RollAmplitude = Maths::Max(RollAmplitude, 0.f);
        Frequency     = Maths::Max(Frequency, 0.f);
        Duration      = Maths::Max(Duration, 0.f);
        BlendIn       = Maths::Max(BlendIn, 0.f);
        BlendOut      = Maths::Max(BlendOut, 0.f);
    }

    // =========================================================================
    // CameraTrauma
    // =========================================================================
    float CameraTrauma::At(const double InTime) const noexcept
    {
        const bool   bLatest = InTime >= Time;
        const float  lLevel  = bLatest ? Level : LevelBefore;
        const double lSince  = Maths::Max(0.0, InTime - (bLatest ? Time : TimeBefore));
        return Maths::Max(0.f, lLevel - Decay * static_cast<float>(lSince));
    }

    void CameraTrauma::Add(const float InAmount, const double InTime) noexcept
    {
        // Raised twice at one time (twice in a step): one level.
        if (InTime > Time)
        {
            const float lNow = At(InTime);
            LevelBefore = Level;
            TimeBefore  = Time;
            Level       = lNow;
            Time        = InTime;
        }

        Level = Maths::Clamp(Level + InAmount, 0.f, 1.f);
    }
}
