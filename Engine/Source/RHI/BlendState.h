#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Maths/Maths.h"
#include "RHI/Pipeline.h"   // EBlendMode

namespace Opaax
{
    // =============================================================================
    // Blend state — what each EBlendMode does to the target, as data a backend maps to its own
    //   calls, and the same maths on the CPU (tests, colours prepared for a premultiplied target).
    //   A blend writes Src * SrcFactor + Dst * DstFactor, colour and alpha each with their own pair.
    // =============================================================================

    /** What a blend multiplies the source or the target by. */
    enum class EBlendFactor : Uint8
    {
        Zero,
        One,
        SrcAlpha,
        OneMinusSrcAlpha
    };

    /** The fixed-function blend of a mode. Not enabled: the source replaces the target. */
    struct BlendFactors
    {
        bool         bEnabled = false;
        EBlendFactor SrcColor = EBlendFactor::One;
        EBlendFactor DstColor = EBlendFactor::Zero;
        EBlendFactor SrcAlpha = EBlendFactor::One;
        EBlendFactor DstAlpha = EBlendFactor::Zero;

        constexpr bool operator==(const BlendFactors&) const noexcept = default;
    };

    /** The factors of InMode. */
    constexpr BlendFactors MakeBlendFactors(const EBlendMode InMode) noexcept
    {
        switch (InMode)
        {
        case EBlendMode::None:
            return BlendFactors{};
        case EBlendMode::Alpha:
            return BlendFactors{ true, EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha,
                                 EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha };
        case EBlendMode::Additive:
            return BlendFactors{ true, EBlendFactor::One, EBlendFactor::One, EBlendFactor::One, EBlendFactor::One };
        case EBlendMode::Paint:
            return BlendFactors{ true, EBlendFactor::SrcAlpha, EBlendFactor::OneMinusSrcAlpha,
                                 EBlendFactor::One, EBlendFactor::OneMinusSrcAlpha };
        }
        return BlendFactors{};
    }

    /** The value of InFactor for a source InSrc. */
    constexpr float BlendFactorValue(const EBlendFactor InFactor, const Vector4F& InSrc) noexcept
    {
        switch (InFactor)
        {
        case EBlendFactor::Zero:             return 0.f;
        case EBlendFactor::One:              return 1.f;
        case EBlendFactor::SrcAlpha:         return InSrc.a;
        case EBlendFactor::OneMinusSrcAlpha: return 1.f - InSrc.a;
        }
        return 0.f;
    }

    /** What the GPU writes when InSrc is blended onto InDst with InFactors (no rounding to 8 bits). */
    inline Vector4F ApplyBlend(const BlendFactors& InFactors, const Vector4F& InSrc, const Vector4F& InDst) noexcept
    {
        if (!InFactors.bEnabled)
        {
            return InSrc;
        }

        const float lSrcColor = BlendFactorValue(InFactors.SrcColor, InSrc);
        const float lDstColor = BlendFactorValue(InFactors.DstColor, InSrc);
        const float lSrcAlpha = BlendFactorValue(InFactors.SrcAlpha, InSrc);
        const float lDstAlpha = BlendFactorValue(InFactors.DstAlpha, InSrc);

        return Vector4F{ InSrc.r * lSrcColor + InDst.r * lDstColor,
                         InSrc.g * lSrcColor + InDst.g * lDstColor,
                         InSrc.b * lSrcColor + InDst.b * lDstColor,
                         InSrc.a * lSrcAlpha + InDst.a * lDstAlpha };
    }

    // =============================================================================
    // Premultiplied colour — the colour already multiplied by its alpha: what a paintable texture
    //   holds, so that painting over it and averaging it into mipmaps are both plain sums.
    // =============================================================================

    /** InStraight with its colour multiplied by its alpha. */
    constexpr Vector4F Premultiply(const Vector4F& InStraight) noexcept
    {
        return Vector4F{ InStraight.r * InStraight.a, InStraight.g * InStraight.a, InStraight.b * InStraight.a,
                         InStraight.a };
    }

    /**
     * The straight colour of a premultiplied one, as the sprite shader reads it: divided by its alpha
     * (at least 1/255, so a transparent texel stays black) and kept at most 1 (8-bit rounding can
     * leave a colour above its alpha).
     */
    inline Vector4F Unpremultiply(const Vector4F& InPremultiplied) noexcept
    {
        const float lAlpha = Maths::Max(InPremultiplied.a, 1.f / 255.f);
        return Vector4F{ Maths::Min(InPremultiplied.r / lAlpha, 1.f), Maths::Min(InPremultiplied.g / lAlpha, 1.f),
                         Maths::Min(InPremultiplied.b / lAlpha, 1.f), InPremultiplied.a };
    }

    /** A premultiplied source over a premultiplied target: Src + Dst * (1 - Src.a), colour and alpha alike. */
    constexpr Vector4F PremultipliedOver(const Vector4F& InSrc, const Vector4F& InDst) noexcept
    {
        const float lKeep = 1.f - InSrc.a;
        return Vector4F{ InSrc.r + InDst.r * lKeep, InSrc.g + InDst.g * lKeep, InSrc.b + InDst.b * lKeep,
                         InSrc.a + InDst.a * lKeep };
    }
}
