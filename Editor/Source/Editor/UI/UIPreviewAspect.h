#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // The UI designer's layout target: the aspect the previewed canvas is laid out at. Header-only and
    //   ImGui-free so tests can check that a named aspect gives exactly that ratio.
    // =============================================================================

    enum class EUIPreviewAspect : Uint8
    {
        Free,     // the framebuffer's own (set by the dock)
        W16x9,
        W21x9,
        W4x3,
        W16x10,
        P9x16     // a phone held upright
    };

    inline constexpr const char* ToString(const EUIPreviewAspect InAspect) noexcept
    {
        switch (InAspect)
        {
            case EUIPreviewAspect::Free:   return "Free";
            case EUIPreviewAspect::W16x9:  return "16:9";
            case EUIPreviewAspect::W21x9:  return "21:9";
            case EUIPreviewAspect::W4x3:   return "4:3";
            case EUIPreviewAspect::W16x10: return "16:10";
            case EUIPreviewAspect::P9x16:  return "9:16";
        }
        return "Free";
    }

    inline constexpr EUIPreviewAspect kUIPreviewAspects[] = {
        EUIPreviewAspect::Free,  EUIPreviewAspect::W16x9, EUIPreviewAspect::W21x9,
        EUIPreviewAspect::W4x3,  EUIPreviewAspect::W16x10, EUIPreviewAspect::P9x16,
    };

    /**
     * The pixel size the canvas is laid out at. Only the ratio matters, so a named aspect is its two
     * integers x 120 (16:9 is exactly 1920x1080). Free uses the framebuffer's size.
     */
    inline constexpr Vector2u32 PreviewLayoutSize(const EUIPreviewAspect InAspect, const Uint32 InFramebufferW,
                                                  const Uint32 InFramebufferH) noexcept
    {
        constexpr Uint32 kScale = 120u;

        switch (InAspect)
        {
            case EUIPreviewAspect::W16x9:  return { 16u * kScale, 9u  * kScale };
            case EUIPreviewAspect::W21x9:  return { 21u * kScale, 9u  * kScale };
            case EUIPreviewAspect::W4x3:   return { 4u  * kScale, 3u  * kScale };
            case EUIPreviewAspect::W16x10: return { 16u * kScale, 10u * kScale };
            case EUIPreviewAspect::P9x16:  return { 9u  * kScale, 16u * kScale };
            case EUIPreviewAspect::Free:   break;
        }
        return { InFramebufferW, InFramebufferH };
    }
}
