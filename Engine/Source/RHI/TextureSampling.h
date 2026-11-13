#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES

namespace Opaax
{
    // =============================================================================
    // ETextureFilter — how the project's colour textures are read when they are drawn smaller or
    //   larger than their pixels. One choice for the whole project (Renderer.config).
    // =============================================================================
    enum class ETextureFilter : Uint8
    {
        /** The nearest texel, always: hard pixels at any size. Shimmers when drawn smaller. */
        Nearest,

        /** Hard pixels when enlarged, blended when reduced. For pixel art. */
        Pixel,

        /** Blended when enlarged, and reduced through smaller copies of the texture (mipmaps). For drawn or painted art. */
        Smooth
    };

    /** Name for logs and the value written in the config. */
    inline const char* ToString(const ETextureFilter InFilter) noexcept
    {
        switch (InFilter)
        {
        case ETextureFilter::Nearest: return "Nearest";
        case ETextureFilter::Pixel:   return "Pixel";
        case ETextureFilter::Smooth:  return "Smooth";
        }
        return "Pixel";
    }

    /** How one texel is picked from its neighbours. */
    enum class ESampleFilter : Uint8
    {
        Nearest,
        Linear
    };

    /** What a read outside the texture gives. */
    enum class ETextureWrap : Uint8
    {
        /** The texel of the nearest edge: a sprite's border takes nothing from the opposite side. */
        Clamp,
        Repeat
    };

    /** How a texture is read: what a backend's sampler is made from. */
    struct SamplerDesc
    {
        /** When the texture is drawn smaller than its pixels. */
        ESampleFilter Reduce  = ESampleFilter::Linear;

        /** When it is drawn larger. */
        ESampleFilter Enlarge = ESampleFilter::Nearest;

        /** Reduced through the texture's smaller copies, blended between the two nearest. */
        bool bMipmaps = false;

        ETextureWrap Wrap = ETextureWrap::Clamp;

        constexpr bool operator==(const SamplerDesc&) const noexcept = default;
    };

    /** The sampler of a project filter. */
    constexpr SamplerDesc MakeSamplerDesc(const ETextureFilter InFilter) noexcept
    {
        switch (InFilter)
        {
        case ETextureFilter::Nearest: return SamplerDesc{ ESampleFilter::Nearest, ESampleFilter::Nearest, false, ETextureWrap::Clamp };
        case ETextureFilter::Pixel:   return SamplerDesc{ ESampleFilter::Linear,  ESampleFilter::Nearest, false, ETextureWrap::Clamp };
        case ETextureFilter::Smooth:  return SamplerDesc{ ESampleFilter::Linear,  ESampleFilter::Linear,  true,  ETextureWrap::Clamp };
        }
        return SamplerDesc{};
    }

    /** How many mipmap levels an image has, itself included: each halves the one before, down to 1x1. */
    constexpr Uint32 MipLevelCount(const Uint32 InWidth, const Uint32 InHeight) noexcept
    {
        Uint32 lLargest = InWidth > InHeight ? InWidth : InHeight;
        Uint32 lLevels  = lLargest > 0 ? 1u : 0u;

        while (lLargest > 1)
        {
            lLargest /= 2;
            ++lLevels;
        }
        return lLevels;
    }

    /** One side of mipmap level InLevel of an image InSize texels long (level 0 is the image). */
    constexpr Uint32 MipLevelSize(const Uint32 InSize, const Uint32 InLevel) noexcept
    {
        const Uint32 lSize = InLevel < 32 ? InSize >> InLevel : 0u;
        return lSize > 0 ? lSize : 1u;
    }
}

OPAAX_ENUM_VALUES(Opaax::ETextureFilter, Nearest, Pixel, Smooth)
