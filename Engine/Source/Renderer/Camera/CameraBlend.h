#pragma once

#include "Core/Maths/Maths.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"
#include "Renderer/CameraView.h"

namespace Opaax
{
    /** How the view eases from one camera to the next over a blend. */
    enum class ECameraBlend : Uint8
    {
        Cut,        // the new camera at once
        Linear,     // at an even pace
        EaseIn,     // leaves slowly, arrives at speed
        EaseOut,    // leaves at speed, arrives slowly
        EaseInOut   // leaves and arrives slowly
    };

    /** Enum to string. */
    inline const char* ToString(const ECameraBlend InBlend) noexcept
    {
        switch (InBlend)
        {
        case ECameraBlend::Cut:       return "Cut";
        case ECameraBlend::Linear:    return "Linear";
        case ECameraBlend::EaseIn:    return "EaseIn";
        case ECameraBlend::EaseOut:   return "EaseOut";
        case ECameraBlend::EaseInOut: return "EaseInOut";
        }

        return "EaseInOut";
    }
}

OPAAX_ENUM_VALUES(Opaax::ECameraBlend, Cut, Linear, EaseIn, EaseOut, EaseInOut)

namespace Opaax
{
    // =============================================================================
    // Blending two views (pure functions): what the director draws while it hands the view from one
    //   camera to the next.
    // =============================================================================

    /**
     * How far the view is toward the new camera (0: the old one, 1: the new one) at InTime, the
     * fraction of the blend gone by (clamped to [0, 1]), along InCurve. A Cut is at 1 at once.
     */
    inline float CameraBlendWeight(const ECameraBlend InCurve, const float InTime) noexcept
    {
        if (InCurve == ECameraBlend::Cut)
        {
            return 1.f;
        }

        const float lTime = Maths::Clamp(InTime, 0.f, 1.f);
        switch (InCurve)
        {
        case ECameraBlend::Linear:    return lTime;
        case ECameraBlend::EaseIn:    return lTime * lTime;
        case ECameraBlend::EaseOut:   return lTime * (2.f - lTime);
        case ECameraBlend::EaseInOut: return lTime * lTime * (3.f - 2.f * lTime);
        case ECameraBlend::Cut:       break;
        }
        return 1.f;
    }

    /**
     * The view InWeight of the way from InFrom to InTo (clamped to [0, 1], each end given back as it
     * is): the position along a line, the size in log space so the zoom feels even (100 to 400 is at
     * 200 halfway), the roll the short way round (170 to -170 degrees turns 20).
     */
    inline CameraView BlendCameraViews(const CameraView& InFrom, const CameraView& InTo, const float InWeight) noexcept
    {
        if (InWeight <= 0.f)
        {
            return InFrom;
        }
        if (InWeight >= 1.f)
        {
            return InTo;
        }

        CameraView lView;
        lView.Position = InFrom.Position + (InTo.Position - InFrom.Position) * InWeight;
        lView.Rotation = InFrom.Rotation + Maths::Remainder(InTo.Rotation - InFrom.Rotation, 360.f) * InWeight;

        // Log space needs two sizes above zero; a size that is not (the guard deals with it) blends linearly.
        lView.OrthoSize = InFrom.OrthoSize > 0.f && InTo.OrthoSize > 0.f
                              ? InFrom.OrthoSize * Maths::Pow(InTo.OrthoSize / InFrom.OrthoSize, InWeight)
                              : InFrom.OrthoSize + (InTo.OrthoSize - InFrom.OrthoSize) * InWeight;
        return lView;
    }
}
