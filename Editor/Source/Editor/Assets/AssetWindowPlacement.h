#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/Maths/Maths.h"       // Max

namespace Opaax::Editor
{
    // =============================================================================
    // Where a new floating asset window opens. Header-only and ImGui-free so tests can check the
    //   placements. Positions are top-left corners, in the same space as the work area.
    // =============================================================================

    /** A window of InSize centred in the work area; at its corner when larger than it. */
    inline Vector2F CenteredWindowPos(const Vector2F InWorkPos, const Vector2F InWorkSize,
                                      const Vector2F InSize) noexcept
    {
        return { Maths::Max(InWorkPos.x, InWorkPos.x + (InWorkSize.x - InSize.x) * 0.5f),
                 Maths::Max(InWorkPos.y, InWorkPos.y + (InWorkSize.y - InSize.y) * 0.5f) };
    }

    /**
     * A window one step (a title bar) below and right of the newest one, so neither hides the other's
     * title. Back to the work area's corner when that would cross its right or bottom edge.
     */
    inline Vector2F CascadedWindowPos(const Vector2F InNewest, const float InStep, const Vector2F InSize,
                                      const Vector2F InWorkPos, const Vector2F InWorkSize) noexcept
    {
        const Vector2F lNext = { InNewest.x + InStep, InNewest.y + InStep };
        const bool     bFits = lNext.x + InSize.x <= InWorkPos.x + InWorkSize.x
                            && lNext.y + InSize.y <= InWorkPos.y + InWorkSize.y;

        return bFits ? lNext : InWorkPos;
    }
}
