#pragma once

#include "Core/OpaaxTypes.h"

#include <imgui.h>

// =============================================================================
// ImguiLayout — GEOMETRY. Pure functions of numbers: no ImGui state read, none written, nothing
//   drawn. The half of the library that is testable without a context.
// =============================================================================
namespace Opaax::Editor::ImguiLayout
{
    /**
     * Fit InWidth x InHeight inside a square of InMax without distorting it.
     *
     * A zero dimension answers a square box rather than dividing by zero — the honest result for an
     * image whose shape is not known yet.
     */
    inline ImVec2 AspectFit(const Uint32 InWidth, const Uint32 InHeight, const float InMax) noexcept
    {
        const float lW = static_cast<float>(InWidth);
        const float lH = static_cast<float>(InHeight);

        const float lAspect = (lH > 0.f && lW > 0.f) ? (lW / lH) : 1.f;

        return (lAspect > 1.f) ? ImVec2(InMax, InMax / lAspect)
                               : ImVec2(InMax * lAspect, InMax);
    }

    /** The square box of side InSide centred inside [InMin, InMax]. */
    inline void CenteredSquare(const ImVec2 InMin, const ImVec2 InMax, const float InSide,
                               ImVec2& OutMin, ImVec2& OutMax) noexcept
    {
        const float lX = (InMin.x + InMax.x - InSide) * 0.5f;
        const float lY = (InMin.y + InMax.y - InSide) * 0.5f;

        OutMin = ImVec2(lX, lY);
        OutMax = ImVec2(lX + InSide, lY + InSide);
    }

    /** [InMin, InMax] shrunk by InFraction of its size on every side. 0.16 is the tile card's inset. */
    inline void Inset(const ImVec2 InMin, const ImVec2 InMax, const float InFraction,
                      ImVec2& OutMin, ImVec2& OutMax) noexcept
    {
        const float lX = (InMax.x - InMin.x) * InFraction;
        const float lY = (InMax.y - InMin.y) * InFraction;

        OutMin = ImVec2(InMin.x + lX, InMin.y + lY);
        OutMax = ImVec2(InMax.x - lX, InMax.y - lY);
    }
}
