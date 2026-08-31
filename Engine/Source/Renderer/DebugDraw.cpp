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
                             float InThickness, ERenderLayer InLayer)
    {
        m_Lines.emplace_back(InStart, InEnd, InColor, InThickness, InLayer);
    }

    // =========================================================================
    // DrawBox — ONE entry, rendered as a single hollow quad.
    //
    // It used to enqueue four segments, corner to corner, whose joints overlapped by half a line
    // width. That was the right call while a quad could only be solid; now that a quad carries the
    // half-extent of its own hole, the border is a property of one quad — a quarter of the geometry
    // and exact corners, with no mitring maths either way.
    // =========================================================================
    void DebugDraw::DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                            float InThickness, ERenderLayer InLayer)
    {
        m_Boxes.emplace_back(InCenter, InSize, InColor, InThickness, InLayer);
    }

    void DebugDraw::DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                               float InThickness, ERenderLayer InLayer)
    {
        DrawBox(InBounds.Center, InBounds.Size(), InColor, InThickness, InLayer);
    }

    void DebugDraw::Clear() noexcept
    {
        m_Lines.clear();
        m_Boxes.clear();
    }
}
