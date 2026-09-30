#include "Renderer/DebugDraw.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace Opaax
{
    namespace
    {
        constexpr float kTwoPi = 6.28318530717958647692f;
        constexpr float kPi    = 3.14159265358979323846f;
    }

    // =========================================================================
    // ToQuad — a line is a thin quad of the segment's length, centred on the midpoint and rotated
    // onto the segment. A zero-length segment gives a zero-width quad (no NaN).
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

        // Fewer than 3 points: nothing.
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

        // Both centres are the same: the capsule is a circle.
        if (lLength < 1e-4f)
        {
            BuildCircleOutline(InCenter1, InRadius, InSegmentsPerCap * 2, OutPoints);
            return;
        }

        OutPoints.reserve(InSegmentsPerCap * 2);

        // Each cap is a half-turn starting perpendicular to the axis, so the flanks come for free.
        const float lAxisAngle = glm::atan(lAxis.y, lAxis.x);
        const float lStep      = kPi / static_cast<float>(InSegmentsPerCap - 1);

        // Cap around centre 2.
        for (Uint32 i = 0; i < InSegmentsPerCap; ++i)
        {
            const float lAngle = lAxisAngle - kPi * 0.5f + lStep * static_cast<float>(i);
            OutPoints.emplace_back(InCenter2.x + InRadius * glm::cos(lAngle),
                                   InCenter2.y + InRadius * glm::sin(lAngle));
        }

        // Cap around centre 1 (closes the polygon).
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
                             float InThickness, ERenderLayer InLayer, DebugChannel InChannel,
                             const World* InSource)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        m_Lines.emplace_back(InStart, InEnd, InColor, InThickness, InLayer, InSource);
    }

    // =========================================================================
    // DrawBox — one hollow quad (exact corners, a quarter of the geometry of four lines).
    // =========================================================================
    void DebugDraw::DrawBox(const Vector2F& InCenter, const Vector2F& InSize, const Vector4F& InColor,
                            float InThickness, ERenderLayer InLayer, DebugChannel InChannel,
                            float InRotationRad, const World* InSource)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        m_Boxes.emplace_back(InCenter, InSize, InColor, InThickness, InRotationRad, InLayer, InSource);
    }

    void DebugDraw::DrawBounds(const Bounds2D& InBounds, const Vector4F& InColor,
                               float InThickness, ERenderLayer InLayer, DebugChannel InChannel,
                               const World* InSource)
    {
        // Bounds are axis-aligned: no rotation.
        DrawBox(InBounds.Center, InBounds.Size(), InColor, InThickness, InLayer, InChannel, 0.f, InSource);
    }

    void DebugDraw::DrawCircle(const Vector2F& InCenter, const float InRadius, const Vector4F& InColor,
                               const float InThickness, const ERenderLayer InLayer,
                               const DebugChannel InChannel, const Uint32 InSegments,
                               const World* InSource)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        BuildCircleOutline(InCenter, InRadius, InSegments, m_OutlineScratch);
        EmitClosedPolygon(InColor, InThickness, InLayer, InSource);
    }

    void DebugDraw::DrawCapsule(const Vector2F& InCenter1, const Vector2F& InCenter2, const float InRadius,
                                const Vector4F& InColor, const float InThickness, const ERenderLayer InLayer,
                                const DebugChannel InChannel, const Uint32 InSegmentsPerCap,
                                const World* InSource)
    {
        if (!IsChannelEnabled(InChannel))
        {
            return;
        }

        BuildCapsuleOutline(InCenter1, InCenter2, InRadius, InSegmentsPerCap, m_OutlineScratch);
        EmitClosedPolygon(InColor, InThickness, InLayer, InSource);
    }

    void DebugDraw::EmitClosedPolygon(const Vector4F& InColor, const float InThickness,
                                      const ERenderLayer InLayer, const World* InSource)
    {
        const size_t lCount = m_OutlineScratch.size();
        if (lCount < 2)
        {
            return;
        }

        // The caller already checked the channel.
        for (size_t i = 0; i < lCount; ++i)
        {
            const Vector2F& lFrom = m_OutlineScratch[i];
            const Vector2F& lTo   = m_OutlineScratch[(i + 1) % lCount];

            m_Lines.emplace_back(lFrom, lTo, InColor, InThickness, InLayer, InSource);
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
