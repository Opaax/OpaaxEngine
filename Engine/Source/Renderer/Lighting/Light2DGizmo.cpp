#include "Renderer/Lighting/Light2DGizmo.h"

#include <algorithm>
#include <cmath>

#include "Core/Maths/Maths.h"
#include "Renderer/Components/Light2DComponent.h"

namespace Opaax
{
    namespace
    {
        /** A full circle's segments. */
        constexpr Uint32 CIRCLE_SEGMENTS = 48;

        /** An arc gets a segment per this many degrees. */
        constexpr float ARC_DEGREES_PER_SEGMENT = 7.5f;

        /** The arrow head's sides, from the shaft, and their length (a share of the arrow's). */
        constexpr float ARROW_HEAD_DEGREES = 150.f;
        constexpr float ARROW_HEAD_SHARE   = 0.25f;

        Vector2F Along(const Vector2F& InFrom, const float InAngleRad, const float InLength)
        {
            return InFrom + Vector2F{ std::cos(InAngleRad), std::sin(InAngleRad) } * InLength;
        }

        void AddArc(const Vector2F& InCenter, const float InRadius, const float InFromRad, const float InToRad,
                    const Uint32 InSegments, TDynArray<GizmoLine2D>& OutLines)
        {
            const float lStep = (InToRad - InFromRad) / static_cast<float>(InSegments);
            for (Uint32 lIndex = 0; lIndex < InSegments; ++lIndex)
            {
                const float lFrom = InFromRad + lStep * static_cast<float>(lIndex);
                OutLines.push_back(GizmoLine2D{ Along(InCenter, lFrom, InRadius), Along(InCenter, lFrom + lStep, InRadius) });
            }
        }
    }

    void BuildLight2DGizmo(const Light2DComponent& InLight, const Vector2F& InPosition, const float InRotationDegrees,
                           const float InArrowLength, TDynArray<GizmoLine2D>& OutLines)
    {
        OutLines.clear();

        const float lRotation = Maths::DegreesToRadians(InRotationDegrees);
        const float lRadius   = std::max(InLight.Radius, 0.f);

        switch (InLight.Type)
        {
        case ELight2DType::Point:
        {
            AddArc(InPosition, lRadius, 0.f, Maths::DegreesToRadians(360.f), CIRCLE_SEGMENTS, OutLines);
            break;
        }
        case ELight2DType::Spot:
        {
            // The same angles as the shader's cone (Lighting2D: PackLights2D).
            const float lHalfDegrees = std::clamp(InLight.ConeAngle, 1.f, 360.f) * 0.5f;
            const float lHalfOuter   = Maths::DegreesToRadians(lHalfDegrees);
            const float lHalfInner   = lHalfOuter * (1.f - std::clamp(InLight.ConeSoftness, 0.f, 1.f));

            OutLines.push_back(GizmoLine2D{ InPosition, Along(InPosition, lRotation - lHalfOuter, lRadius) });
            OutLines.push_back(GizmoLine2D{ InPosition, Along(InPosition, lRotation + lHalfOuter, lRadius) });

            const Uint32 lSegments = std::max(2u, static_cast<Uint32>(std::ceil(2.f * lHalfDegrees / ARC_DEGREES_PER_SEGMENT)));
            AddArc(InPosition, lRadius, lRotation - lHalfOuter, lRotation + lHalfOuter, lSegments, OutLines);

            // Where the light is full, when it fades before the edge.
            if (lHalfInner > 0.f && lHalfInner < lHalfOuter)
            {
                OutLines.push_back(GizmoLine2D{ InPosition, Along(InPosition, lRotation - lHalfInner, lRadius) });
                OutLines.push_back(GizmoLine2D{ InPosition, Along(InPosition, lRotation + lHalfInner, lRadius) });
            }
            break;
        }
        case ELight2DType::Global:
        {
            const Vector2F lTip  = Along(InPosition, lRotation, InArrowLength);
            const float    lSide = Maths::DegreesToRadians(ARROW_HEAD_DEGREES);

            OutLines.push_back(GizmoLine2D{ InPosition, lTip });
            OutLines.push_back(GizmoLine2D{ lTip, Along(lTip, lRotation + lSide, InArrowLength * ARROW_HEAD_SHARE) });
            OutLines.push_back(GizmoLine2D{ lTip, Along(lTip, lRotation - lSide, InArrowLength * ARROW_HEAD_SHARE) });
            break;
        }
        }
    }
}
