#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"

namespace Opaax
{
    /**
     * The shape of a motion from 0 to 1. In starts slowly, Out ends slowly, InOut does both. Back
     * overshoots (In dips below 0 first, Out passes 1 before settling), Elastic springs past 1, Bounce
     * lands in hops.
     */
    enum class EEase : Uint8
    {
        Linear,
        QuadIn,
        QuadOut,
        QuadInOut,
        CubicIn,
        CubicOut,
        CubicInOut,
        SineIn,
        SineOut,
        SineInOut,
        BackIn,
        BackOut,
        BackInOut,
        ElasticOut,
        BounceOut
    };

    /** Enum to string: the name saved in files. */
    inline const char* ToString(const EEase InEase) noexcept
    {
        switch (InEase)
        {
        case EEase::Linear:     return "Linear";
        case EEase::QuadIn:     return "QuadIn";
        case EEase::QuadOut:    return "QuadOut";
        case EEase::QuadInOut:  return "QuadInOut";
        case EEase::CubicIn:    return "CubicIn";
        case EEase::CubicOut:   return "CubicOut";
        case EEase::CubicInOut: return "CubicInOut";
        case EEase::SineIn:     return "SineIn";
        case EEase::SineOut:    return "SineOut";
        case EEase::SineInOut:  return "SineInOut";
        case EEase::BackIn:     return "BackIn";
        case EEase::BackOut:    return "BackOut";
        case EEase::BackInOut:  return "BackInOut";
        case EEase::ElasticOut: return "ElasticOut";
        case EEase::BounceOut:  return "BounceOut";
        }

        return "Linear";
    }
}

OPAAX_ENUM_VALUES(Opaax::EEase, Linear, QuadIn, QuadOut, QuadInOut, CubicIn, CubicOut, CubicInOut,
                  SineIn, SineOut, SineInOut, BackIn, BackOut, BackInOut, ElasticOut, BounceOut)

// =============================================================================
// Easing — the curves of EEase, as pure functions (Penner's equations).
// =============================================================================
namespace Opaax::Easing
{
    /**
     * How far along InEase is at InTime, the fraction of the motion gone by. InTime is clamped to
     * [0, 1]; the result is exactly 0 at 0 and exactly 1 at 1, and may leave [0, 1] between (Back,
     * Elastic).
     */
    float Evaluate(EEase InEase, float InTime) noexcept;
}
