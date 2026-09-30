#pragma once

#include <cmath>

#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    // =============================================================================
    // Bounds2D — axis-aligned box in world units: centre and half-extent.
    // =============================================================================
    struct Bounds2D
    {
        Vector2F Center     = { 0.f, 0.f };
        Vector2F HalfExtent = { 0.f, 0.f };

        // =============================================================================
        // Construction
        // =============================================================================

        /**
         * Half-extent is always positive: a negative Scale (flip) must not break Contains.
         */
        static Bounds2D FromCenterSize(const Vector2F& InCenter, const Vector2F& InSize) noexcept
        {
            return Bounds2D{ InCenter, { std::fabs(InSize.x) * 0.5f, std::fabs(InSize.y) * 0.5f } };
        }

        /**
         * The axis-aligned box that covers a rotated box.
         */
        static Bounds2D FromCenterSizeRotated(const Vector2F& InCenter, const Vector2F& InSize,
                                              float InRotationRad) noexcept
        {
            const float lCos = std::fabs(std::cos(InRotationRad));
            const float lSin = std::fabs(std::sin(InRotationRad));
            const float lHx  = std::fabs(InSize.x) * 0.5f;
            const float lHy  = std::fabs(InSize.y) * 0.5f;

            return Bounds2D{ InCenter, { lHx * lCos + lHy * lSin, lHx * lSin + lHy * lCos } };
        }

        /** From two opposite corners, in any order. */
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

        /** Full width and height. */
        Vector2F Size() const noexcept { return { HalfExtent.x * 2.f, HalfExtent.y * 2.f }; }

        /** Inclusive: a point on the edge is inside. */
        bool Contains(const Vector2F& InPoint) const noexcept
        {
            return std::fabs(InPoint.x - Center.x) <= HalfExtent.x
                && std::fabs(InPoint.y - Center.y) <= HalfExtent.y;
        }

        /** Inclusive: touching counts. */
        bool Intersects(const Bounds2D& InOther) const noexcept
        {
            return std::fabs(InOther.Center.x - Center.x) <= HalfExtent.x + InOther.HalfExtent.x
                && std::fabs(InOther.Center.y - Center.y) <= HalfExtent.y + InOther.HalfExtent.y;
        }

        /** Grows to also cover InOther. */
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
