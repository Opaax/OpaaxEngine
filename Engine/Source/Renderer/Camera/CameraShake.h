#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Resources/Asset/Asset.h"

namespace Opaax
{
    /** What a camera shake moves along. */
    enum class ECameraShakePattern : Uint8
    {
        Wave,   // a steady sine at Frequency: a rumble, an engine
        Noise   // smooth noise at Frequency: a hit, an explosion, trauma
    };

    /** Enum to string. */
    inline const char* ToString(const ECameraShakePattern InPattern) noexcept
    {
        switch (InPattern)
        {
        case ECameraShakePattern::Wave:  return "Wave";
        case ECameraShakePattern::Noise: return "Noise";
        }

        return "Noise";
    }
}

OPAAX_ENUM_VALUES(Opaax::ECameraShakePattern, Wave, Noise)

namespace Opaax
{
    // =============================================================================
    // CameraShakeOffset — how far a shake moves the view at a moment.
    // =============================================================================
    struct CameraShakeOffset
    {
        /** World units, along the view's own axes (x right, y up on screen). */
        Vector2F Position = { 0.f, 0.f };

        /** Degrees, counter-clockwise. */
        float Rotation = 0.f;
    };

    // =============================================================================
    // CameraShake — how a view shakes: its pattern, how far and how fast, how long, and how it fades in
    //   and out. Played by CameraShakeSubsystem or a CameraShakeSourceComponent. A shake is a function of
    //   the time since it started and of its seed, so it replays the same at any frame rate.
    // =============================================================================
    class CameraShake final : public Asset
    {
        OPAAX_OBJECT(CameraShake, Asset)

    public:
        ECameraShakePattern Pattern = ECameraShakePattern::Noise;

        /** How far the view moves at most, in world units. */
        float Amplitude = 8.f;

        /** How far the view turns at most, in degrees. */
        float RollAmplitude = 0.f;

        /** Shakes a second. */
        float Frequency = 12.f;

        /** Seconds it lasts, its fades included. 0: until stopped. */
        float Duration = 0.5f;

        /** Seconds it takes to reach full strength, and to fade out at its end or once stopped. */
        float BlendIn  = 0.05f;
        float BlendOut = 0.25f;

        OPAAX_PROPERTIES(CameraShake,
                         OPAAX_PROP(Pattern).SetTooltip("Wave: a steady sine (a rumble).\n"
                                                        "Noise: smooth noise (a hit, an explosion, trauma)."),
                         OPAAX_PROP(Amplitude).SetRange(0.f, 500.f).SetTooltip("How far the view moves at most, in world units."),
                         OPAAX_PROP(RollAmplitude).SetRange(0.f, 45.f).SetTooltip("How far the view turns at most, in degrees."),
                         OPAAX_PROP(Frequency).SetRange(0.f, 60.f).SetTooltip("Shakes a second."),
                         OPAAX_PROP(Duration).SetRange(0.f, 30.f).SetTooltip("Seconds it lasts, its fades included.\n"
                                                                             "0: until stopped (a trauma source's shake)."),
                         OPAAX_PROP(BlendIn).SetRange(0.f, 5.f).SetTooltip("Seconds it takes to reach full strength."),
                         OPAAX_PROP(BlendOut).SetRange(0.f, 5.f).SetTooltip("Seconds it fades out over, at its end or once stopped."))

        /**
         * Seconds after its start when it is over: at its Duration, or BlendOut after InStoppedAt (seconds
         * after its start) when stopped sooner. Infinite for an endless shake not stopped.
         */
        double EndTime(TOptional<double> InStoppedAt) const noexcept;

        /** Its strength, 0 to 1, InElapsed seconds after its start: its fades in and out, and its stop. */
        float Envelope(double InElapsed, TOptional<double> InStoppedAt) const noexcept;

        /** Where the pattern is InElapsed seconds after its start for InSeed, at full strength. */
        CameraShakeOffset Oscillate(Uint32 InSeed, double InElapsed) const noexcept;

        /** Oscillate times Envelope: nothing before its start or after its end. */
        CameraShakeOffset Sample(Uint32 InSeed, double InElapsed, TOptional<double> InStoppedAt) const noexcept;

        //~Begin Object interface
        /** Keeps the numbers usable: none below 0. */
        void PostEditChange() override;
        //~End Object interface
    };

    /**
     * How much of a shake reaches a view InDistance away from its source: all of it within InInner, none
     * from InOuter, linearly between. An InOuter of 0 or less: all of it, anywhere.
     */
    inline float CameraShakeAttenuation(const float InDistance, const float InInner, const float InOuter) noexcept
    {
        if (InOuter <= 0.f || InDistance <= InInner)
        {
            return 1.f;
        }

        if (InDistance >= InOuter)
        {
            return 0.f;
        }

        return (InOuter - InDistance) / (InOuter - InInner);
    }

    // =============================================================================
    // CameraTrauma — how shaken a source is, 0 to 1 (Squirrel Eiserloh's trauma): hits raise it, it falls
    //   by Decay a second, and its shake plays at its square. Kept as the level set at a time, and the
    //   one set before it, and read in closed form: a frame reads it at its own game time.
    // =============================================================================
    struct CameraTrauma
    {
        /** Lost a second. */
        float Decay = 1.f;

        /** The level set at Time, and the one before it, for the frames drawn before Time. */
        float  Level       = 0.f;
        double Time        = 0.0;
        float  LevelBefore = 0.f;
        double TimeBefore  = 0.0;

        /** The trauma at InTime: the last level set by then, less Decay a second since. */
        float At(double InTime) const noexcept;

        /** Raises it by InAmount (lowers it, negative) at InTime, kept in [0, 1]. */
        void Add(float InAmount, double InTime) noexcept;
    };
}
