#pragma once

#include <cmath>

#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // Bounds2D — an axis-aligned box in world units, as a centre and a HALF-extent. Centre-based
    //   because everything that produces one is centre-based: a quad, a sprite and DebugDraw::DrawBox
    //   all take a centre and a full size.
    //
    //   Stateless value type, so header-only and NOT OPAAX_API (I6). It answers the four questions
    //   the editor asks of an entity: is this point inside it (click), does it overlap this region
    //   (marquee), what box covers both of us (focus on several), and what box did this drag draw.
    // =============================================================================
    struct Bounds2D
    {
        Vector2F Center     = { 0.f, 0.f };
        Vector2F HalfExtent = { 0.f, 0.f };

        // =============================================================================
        // Construction
        // =============================================================================

        static Bounds2D FromCenterSize(const Vector2F& InCenter, const Vector2F& InSize) noexcept
        {
            return Bounds2D{ InCenter, { InSize.x * 0.5f, InSize.y * 0.5f } };
        }

        /**
         * The AABB that covers a rotated box — NOT the box itself.
         *
         * A picked sprite draws rotated, so its axis-aligned cover has to grow with the angle or a
         * turned sprite would stop being clickable at its own corners.
         */
        static Bounds2D FromCenterSizeRotated(const Vector2F& InCenter, const Vector2F& InSize,
                                              float InRotationRad) noexcept
        {
            const float lCos = std::fabs(std::cos(InRotationRad));
            const float lSin = std::fabs(std::sin(InRotationRad));
            const float lHx  = InSize.x * 0.5f;
            const float lHy  = InSize.y * 0.5f;

            return Bounds2D{ InCenter, { lHx * lCos + lHy * lSin, lHx * lSin + lHy * lCos } };
        }

        /** Two opposite corners in any order — a drag runs in any of four directions. */
        static Bounds2D FromMinMax(const Vector2F& InA, const Vector2F& InB) noexcept
        {
            const Vector2F lMin{ InA.x < InB.x ? InA.x : InB.x, InA.y < InB.y ? InA.y : InB.y };
            const Vector2F lMax{ InA.x > InB.x ? InA.x : InB.x, InA.y > InB.y ? InA.y : InB.y };

            return Bounds2D{ { (lMin.x + lMax.x) * 0.5f, (lMin.y + lMax.y) * 0.5f },
                             { (lMax.x - lMin.x) * 0.5f, (lMax.y - lMin.y) * 0.5f } };
        }

        // =============================================================================
        // Queries
        // =============================================================================

        Vector2F Min() const noexcept { return { Center.x - HalfExtent.x, Center.y - HalfExtent.y }; }
        Vector2F Max() const noexcept { return { Center.x + HalfExtent.x, Center.y + HalfExtent.y }; }

        /** FULL width and height — the form DebugDraw::DrawBox takes. */
        Vector2F Size() const noexcept { return { HalfExtent.x * 2.f, HalfExtent.y * 2.f }; }

        /** Inclusive on the edge: clicking a sprite's exact border should hit it. */
        bool Contains(const Vector2F& InPoint) const noexcept
        {
            return std::fabs(InPoint.x - Center.x) <= HalfExtent.x
                && std::fabs(InPoint.y - Center.y) <= HalfExtent.y;
        }

        /** Touching counts, for the same reason Contains is inclusive. */
        bool Intersects(const Bounds2D& InOther) const noexcept
        {
            return std::fabs(InOther.Center.x - Center.x) <= HalfExtent.x + InOther.HalfExtent.x
                && std::fabs(InOther.Center.y - Center.y) <= HalfExtent.y + InOther.HalfExtent.y;
        }

        /** Grow to cover InOther as well. */
        void Encapsulate(const Bounds2D& InOther) noexcept
        {
            const Vector2F lMin = Min();
            const Vector2F lMax = Max();
            const Vector2F lOMin = InOther.Min();
            const Vector2F lOMax = InOther.Max();

            *this = FromMinMax({ lMin.x < lOMin.x ? lMin.x : lOMin.x, lMin.y < lOMin.y ? lMin.y : lOMin.y },
                               { lMax.x > lOMax.x ? lMax.x : lOMax.x, lMax.y > lOMax.y ? lMax.y : lOMax.y });
        }
    };
}
