#pragma once

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    struct Light2DComponent;

    /** One segment of a gizmo, world units. */
    struct GizmoLine2D
    {
        Vector2F Start = { 0.f, 0.f };
        Vector2F End   = { 0.f, 0.f };
    };

    /**
     * The outline of where a light reaches, for editors: a point light's radius circle, a spot
     * light's cone (with the edges of its full-light inner cone), or an arrow InArrowLength long
     * along a global light's direction.
     * @param InRotationDegrees The entity's world rotation: where spot and global lights point
     */
    void BuildLight2DGizmo(const Light2DComponent& InLight, const Vector2F& InPosition, float InRotationDegrees,
                           float InArrowLength, TDynArray<GizmoLine2D>& OutLines);
}
