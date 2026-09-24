#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    // =============================================================================
    // The UI designer's LAYOUT TARGET — what aspect the previewed canvas lays out at, now that
    //   the VIEW (zoom, pan) no longer has to be the whole canvas (U13).
    //
    //   Header-only and ImGui-free, EditorRectGeometry's idiom, so the one piece of arithmetic
    //   is asserted without a panel: a named aspect must lay the canvas out EXACTLY at that ratio,
    //   or a corner-anchored widget would sit a pixel off where the game puts it.
    // =============================================================================

    enum class EUIPreviewAspect : Uint8
    {
        Free,     // the framebuffer's own — the dock decides, today's behaviour
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
     * The pixel size the canvas is told it is shown in — only its RATIO matters to the layout
     * (UI2), so a named aspect is its two integers × 120: 16:9 is exactly 1920×1080, never a
     * rounded 1919. Free is the framebuffer's own size.
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
