#pragma once

#include <cmath>

#include "Core/Maths/MathTypes.h"
#include "Core/OpaaxTypes.h"

namespace Opaax::Editor
{
    /** Which part of the gizmo a cursor is on. `Both` is the centre square — a free move. */
    enum class EGizmoHandle : Uint8
    {
        None,
        AxisX,
        AxisY,
        Both
    };

    // =============================================================================
    // GizmoLayout — WHERE the handles are, in WORLD units, for one pivot at one zoom.
    //
    //   ONE layout, two readers: ViewportPanel::EnqueueGizmo draws it and Pick hit-tests it. That is
    //   SEL4's rule (the entity icon's size is one value feeding both the DebugDraw box and
    //   EntityQuery::PickAt) applied to the gizmo — what you see is what you grab by construction,
    //   not by two sets of constants being kept in step.
    //
    //   Every field comes from a PIXEL constant times InWorldPerPixel, which is why the gizmo holds
    //   its apparent size at any zoom. Unity spells the same idea HandleUtility.GetHandleSize; the
    //   difference here is that the geometry stays in world space, because a 2D overlay has nothing
    //   to depth-sort against and the engine already draws its overlays that way.
    // =============================================================================
    struct GizmoLayout
    {
        Vector2F Pivot      = { 0.f, 0.f };
        float    AxisLength = 0.f;   // pivot -> arrow tip
        float    AxisGrab   = 0.f;   // half-thickness of an axis's grab band
        float    HeadLength = 0.f;   // arrow head stroke
        float    CenterHalf = 0.f;   // half-side of the free-move square

        Vector2F TipX() const noexcept { return { Pivot.x + AxisLength, Pivot.y }; }
        Vector2F TipY() const noexcept { return { Pivot.x, Pivot.y + AxisLength }; }
    };

    namespace GizmoHandles
    {
        // Screen pixels. Tuned against the Sandbox's 120x120 quads: the arrows reach well clear of a
        // quad's edge, and the centre square stays small enough not to swallow the axes.
        constexpr float AXIS_LENGTH_PX = 68.f;
        constexpr float AXIS_GRAB_PX   = 7.f;
        constexpr float HEAD_PX        = 13.f;
        constexpr float CENTER_HALF_PX = 15.f;

        inline GizmoLayout MakeLayout(const Vector2F& InPivot, const float InWorldPerPixel) noexcept
        {
            return GizmoLayout{ InPivot,
                                AXIS_LENGTH_PX * InWorldPerPixel,
                                AXIS_GRAB_PX   * InWorldPerPixel,
                                HEAD_PX        * InWorldPerPixel,
                                CENTER_HALF_PX * InWorldPerPixel };
        }

        /**
         * The handle under InCursor, or None.
         *
         * The centre square is tested FIRST because it overlaps the root of both axes, and a free
         * move is the safer reading of an ambiguous grab: an author who meant an axis and got a free
         * move sees it immediately, where the reverse silently refuses half their motion.
         */
        inline EGizmoHandle Pick(const GizmoLayout& InLayout, const Vector2F& InCursor) noexcept
        {
            const float lDx = InCursor.x - InLayout.Pivot.x;
            const float lDy = InCursor.y - InLayout.Pivot.y;

            if (std::fabs(lDx) <= InLayout.CenterHalf && std::fabs(lDy) <= InLayout.CenterHalf)
            {
                return EGizmoHandle::Both;
            }

            // The band runs from the pivot to the TIP, so the arrow head is grabbable too.
            if (lDx >= 0.f && lDx <= InLayout.AxisLength && std::fabs(lDy) <= InLayout.AxisGrab)
            {
                return EGizmoHandle::AxisX;
            }

            if (lDy >= 0.f && lDy <= InLayout.AxisLength && std::fabs(lDx) <= InLayout.AxisGrab)
            {
                return EGizmoHandle::AxisY;
            }

            return EGizmoHandle::None;
        }

        /** InDelta reduced to what InHandle permits. None yields no motion at all. */
        inline Vector2F Constrain(const EGizmoHandle InHandle, const Vector2F& InDelta) noexcept
        {
            switch (InHandle)
            {
            case EGizmoHandle::AxisX: return { InDelta.x, 0.f };
            case EGizmoHandle::AxisY: return { 0.f, InDelta.y };
            case EGizmoHandle::Both:  return InDelta;
            case EGizmoHandle::None:  break;
            }

            return { 0.f, 0.f };
        }
    }
}
