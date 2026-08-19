#include "Renderer/DebugDraw.h"

#include <glm/geometric.hpp>       // glm::length
#include <glm/trigonometric.hpp>   // glm::atan

namespace Opaax
{
    // =========================================================================
    // ToQuad — the whole line-rendering trick: a segment is a quad of the segment's length,
    // as thick as the line, centred on the midpoint and rotated onto the segment's direction.
    // That is why DebugDraw needs no new RHI primitive, shader or vertex layout.
    //
    // glm::atan(0, 0) is 0 (IEEE atan2), so a degenerate zero-length segment produces a
    // zero-width quad — invisible — rather than a NaN rotation that would poison the batch.
    // =========================================================================
    DebugQuad ToQuad(const DebugLine& InLine) noexcept
    {
        const Vector2F lDelta = InLine.End - InLine.Start;

        DebugQuad lQuad;
        lQuad.Center      = (InLine.Start + InLine.End) * 0.5f;
        lQuad.Size        = { glm::length(lDelta), InLine.Thickness };
        lQuad.RotationRad = glm::atan(lDelta.y, lDelta.x);

        return lQuad;
    }

    void DebugDraw::DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                             float InThickness)
    {
        m_Lines.emplace_back(InStart, InEnd, InColor, InThickness);
    }

    // =========================================================================
    // DrawBox — four segments, corner to corner, clockwise from the bottom-left. The corners are
    // shared between adjacent segments, so the joints overlap by half a line width; at debug-overlay
    // thicknesses that reads as a clean corner and costs nothing (no mitring maths).
    // =========================================================================
    void DebugDraw::DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                            float InThickness)
    {
        const Vector2F lHalf = InSize * 0.5f;

        const Vector2F lBottomLeft  = { InCenter.x - lHalf.x, InCenter.y - lHalf.y };
        const Vector2F lBottomRight = { InCenter.x + lHalf.x, InCenter.y - lHalf.y };
        const Vector2F lTopRight    = { InCenter.x + lHalf.x, InCenter.y + lHalf.y };
        const Vector2F lTopLeft     = { InCenter.x - lHalf.x, InCenter.y + lHalf.y };

        DrawLine(lBottomLeft,  lBottomRight, InColor, InThickness);
        DrawLine(lBottomRight, lTopRight,    InColor, InThickness);
        DrawLine(lTopRight,    lTopLeft,     InColor, InThickness);
        DrawLine(lTopLeft,     lBottomLeft,  InColor, InThickness);
    }

    void DebugDraw::Clear() noexcept
    {
        m_Lines.clear();
    }
}
