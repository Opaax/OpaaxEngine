#include "Renderer/DebugDraw.h"

#include <glm/geometric.hpp>       // glm::length, glm::normalize
#include <glm/trigonometric.hpp>   // glm::atan, glm::cos, glm::sin

namespace Opaax
{
    namespace
    {
        constexpr float kTwoPi = 6.28318530717958647692f;
        constexpr float kPi    = 3.14159265358979323846f;
    }

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

    // =========================================================================
    // Outline geometry
    // =========================================================================
    void BuildCircleOutline(const Vector2F InCenter, const float InRadius, const Uint32 InSegments,
                            TDynArray<Vector2F>& OutPoints)
    {
        OutPoints.clear();

        // Two points cannot enclose anything; a caller asking for fewer gets nothing rather than a
        // degenerate shape it would have to recognise.
        if (InSegments < 3)
        {
            return;
        }

        OutPoints.reserve(InSegments);

        const float lStep = kTwoPi / static_cast<float>(InSegments);
        for (Uint32 i = 0; i < InSegments; ++i)
        {
            const float lAngle = lStep * static_cast<float>(i);
            OutPoints.emplace_back(InCenter.x + InRadius * glm::cos(lAngle),
                                   InCenter.y + InRadius * glm::sin(lAngle));
        }
    }

    void BuildCapsuleOutline(const Vector2F InCenter1, const Vector2F InCenter2, const float InRadius,
                             const Uint32 InSegmentsPerCap, TDynArray<Vector2F>& OutPoints)
    {
        OutPoints.clear();

        if (InSegmentsPerCap < 2)
        {
            return;
        }

        const Vector2F lAxis   = InCenter2 - InCenter1;
        const float    lLength = glm::length(lAxis);

        // Degenerate: the two centres coincide, so the capsule IS a circle. Answering with one
        // beats answering with a zero-length capsule the caller would have to special-case.
        if (lLength < 1e-4f)
        {
            BuildCircleOutline(InCenter1, InRadius, InSegmentsPerCap * 2, OutPoints);
            return;
        }

        OutPoints.reserve(InSegmentsPerCap * 2);

        // The axis angle: each cap is a half-turn starting perpendicular to it, so the flanks fall
        // out of the traversal instead of needing to be added.
        const float lAxisAngle = glm::atan(lAxis.y, lAxis.x);
        const float lStep      = kPi / static_cast<float>(InSegmentsPerCap - 1);

        // Cap around centre 2, sweeping from one flank to the other.
        for (Uint32 i = 0; i < InSegmentsPerCap; ++i)
        {
            const float lAngle = lAxisAngle - kPi * 0.5f + lStep * static_cast<float>(i);
            OutPoints.emplace_back(InCenter2.x + InRadius * glm::cos(lAngle),
                                   InCenter2.y + InRadius * glm::sin(lAngle));
        }

        // Cap around centre 1, the opposite half-turn — which closes the polygon.
        for (Uint32 i = 0; i < InSegmentsPerCap; ++i)
        {
            const float lAngle = lAxisAngle + kPi * 0.5f + lStep * static_cast<float>(i);
            OutPoints.emplace_back(InCenter1.x + InRadius * glm::cos(lAngle),
                                   InCenter1.y + InRadius * glm::sin(lAngle));
        }
    }

    // =========================================================================
    // Draw calls
    // =========================================================================
    void DebugDraw::DrawLine(const Vector2F& InStart, const Vector2F& InEnd, const Vector4F& InColor,
                             float InThickness, ERenderLayer InLayer, DebugChannel InChannel)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

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
                            float InThickness, ERenderLayer InLayer, DebugChannel InChannel,
                            float InRotationRad)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        m_Boxes.emplace_back(InCenter, InSize, InColor, InThickness, InRotationRad, InLayer);
    }

    void DebugDraw::DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                               float InThickness, ERenderLayer InLayer, DebugChannel InChannel)
    {
        // Bounds are axis-aligned by definition, so no rotation can reach this one.
        DrawBox(InBounds.Center, InBounds.Size(), InColor, InThickness, InLayer, InChannel);
    }

    void DebugDraw::DrawCircle(const Vector2F& InCenter, const float InRadius, const Vector4F& InColor,
                               const float InThickness, const ERenderLayer InLayer,
                               const DebugChannel InChannel, const Uint32 InSegments)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        BuildCircleOutline(InCenter, InRadius, InSegments, m_OutlineScratch);
        EmitClosedPolygon(InColor, InThickness, InLayer);
    }

    void DebugDraw::DrawCapsule(const Vector2F& InCenter1, const Vector2F& InCenter2, const float InRadius,
                                const Vector4F& InColor, const float InThickness, const ERenderLayer InLayer,
                                const DebugChannel InChannel, const Uint32 InSegmentsPerCap)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        BuildCapsuleOutline(InCenter1, InCenter2, InRadius, InSegmentsPerCap, m_OutlineScratch);
        EmitClosedPolygon(InColor, InThickness, InLayer);
    }

    void DebugDraw::EmitClosedPolygon(const Vector4F& InColor, const float InThickness,
                                      const ERenderLayer InLayer)
    {
        const size_t lCount = m_OutlineScratch.size();
        if (lCount < 2)
        {
            return;
        }

        // The channel was already checked by the caller, so these go straight into the queue —
        // re-testing it per segment would ask the same question twenty-four times.
        for (size_t i = 0; i < lCount; ++i)
        {
            const Vector2F& lFrom = m_OutlineScratch[i];
            const Vector2F& lTo   = m_OutlineScratch[(i + 1) % lCount];

            m_Lines.emplace_back(lFrom, lTo, InColor, InThickness, InLayer);
        }
    }

    // =========================================================================
    // Channels
    // =========================================================================
    void DebugDraw::SetChannelEnabled(const DebugChannel InChannel, const bool bInEnabled)
    {
        if (bInEnabled)
        {
            m_DisabledChannels.erase(InChannel.GetId());
        }
        else
        {
            m_DisabledChannels.insert(InChannel.GetId());
        }
    }

    bool DebugDraw::IsChannelEnabled(const DebugChannel InChannel) const noexcept
    {
        return m_DisabledChannels.find(InChannel.GetId()) == m_DisabledChannels.end();
    }

    void DebugDraw::Clear() noexcept
    {
        m_Lines.clear();
        m_Boxes.clear();
    }
}
