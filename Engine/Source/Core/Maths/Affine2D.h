#pragma once

#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/Maths.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    /** What Affine2D::Decompose reads back: a turn in degrees (counter-clockwise), then a scale. */
    struct Affine2DParts
    {
        Vector2F Translation = { 0.f, 0.f };
        float    Rotation    = 0.f;
        Vector2F Scale       = { 1.f, 1.f };
    };

    // =============================================================================
    // Affine2D — a 2D transform: a linear part (where the x and y axes go) and a translation. A point
    //   p goes to XAxis * p.x + YAxis * p.y + Translation. Used for the UI's poses, which turn and
    //   scale widgets about a pivot without touching their layout.
    // =============================================================================
    struct Affine2D
    {
        Vector2F XAxis       = { 1.f, 0.f };
        Vector2F YAxis       = { 0.f, 1.f };
        Vector2F Translation = { 0.f, 0.f };

        // =============================================================================
        // Construction
        // =============================================================================

        static Affine2D Identity() noexcept { return Affine2D{}; }

        /**
         * Scale, then turn by InRotation degrees counter-clockwise, then move by InTranslation.
         */
        static Affine2D FromTranslationRotationScale(const Vector2F& InTranslation, const float InRotation,
                                                     const Vector2F& InScale) noexcept
        {
            const float lRadians = Maths::DegreesToRadians(InRotation);
            const float lCos     = Maths::Cos(lRadians);
            const float lSin     = Maths::Sin(lRadians);

            Affine2D lResult;
            lResult.XAxis       = { lCos * InScale.x, lSin * InScale.x };
            lResult.YAxis       = { -lSin * InScale.y, lCos * InScale.y };
            lResult.Translation = InTranslation;
            return lResult;
        }

        /**
         * Scale and turn about InPivot (which stays put), then move by InTranslation.
         */
        static Affine2D FromPose(const Vector2F& InTranslation, const float InRotation, const Vector2F& InScale,
                                 const Vector2F& InPivot) noexcept
        {
            Affine2D lResult = FromTranslationRotationScale({ 0.f, 0.f }, InRotation, InScale);
            const Vector2F lPivotMoved = lResult.TransformVector(InPivot);
            lResult.Translation = { InPivot.x - lPivotMoved.x + InTranslation.x,
                                    InPivot.y - lPivotMoved.y + InTranslation.y };
            return lResult;
        }

        // =============================================================================
        // Composition
        // =============================================================================

        /**
         * InOther first, then this one: (A * B).TransformPoint(p) is A.TransformPoint(B.TransformPoint(p)).
         * A parent's transform times its child's gives the child's in the parent's space.
         */
        Affine2D operator*(const Affine2D& InOther) const noexcept
        {
            Affine2D lResult;
            lResult.XAxis       = TransformVector(InOther.XAxis);
            lResult.YAxis       = TransformVector(InOther.YAxis);
            lResult.Translation = TransformPoint(InOther.Translation);
            return lResult;
        }

        /** The area scale of the linear part; negative when it flips. */
        float Determinant() const noexcept { return XAxis.x * YAxis.y - YAxis.x * XAxis.y; }

        /**
         * The transform that undoes this one. False, OutInverse untouched, when the transform flattens
         * the plane (a scale of 0): no point maps back.
         */
        bool Inverse(Affine2D& OutInverse) const noexcept
        {
            const float lDeterminant = Determinant();
            if (Maths::IsNearlyZero(lDeterminant))
            {
                return false;
            }

            const float lInv = 1.f / lDeterminant;
            Affine2D lResult;
            lResult.XAxis = { YAxis.y * lInv, -XAxis.y * lInv };
            lResult.YAxis = { -YAxis.x * lInv, XAxis.x * lInv };

            const Vector2F lBack = lResult.TransformVector(Translation);
            lResult.Translation  = { -lBack.x, -lBack.y };

            OutInverse = lResult;
            return true;
        }

        // =============================================================================
        // Applying
        // =============================================================================

        Vector2F TransformPoint(const Vector2F& InPoint) const noexcept
        {
            return { XAxis.x * InPoint.x + YAxis.x * InPoint.y + Translation.x,
                     XAxis.y * InPoint.x + YAxis.y * InPoint.y + Translation.y };
        }

        /** A direction or an offset: the translation does not apply. */
        Vector2F TransformVector(const Vector2F& InVector) const noexcept
        {
            return { XAxis.x * InVector.x + YAxis.x * InVector.y,
                     XAxis.y * InVector.x + YAxis.y * InVector.y };
        }

        /** The axis-aligned box that covers InBounds once transformed. */
        Bounds2D TransformBox(const Bounds2D& InBounds) const noexcept
        {
            const float lHx = Maths::Abs(XAxis.x) * InBounds.HalfExtent.x + Maths::Abs(YAxis.x) * InBounds.HalfExtent.y;
            const float lHy = Maths::Abs(XAxis.y) * InBounds.HalfExtent.x + Maths::Abs(YAxis.y) * InBounds.HalfExtent.y;
            return Bounds2D{ TransformPoint(InBounds.Center), { lHx, lHy } };
        }

        // =============================================================================
        // Queries
        // =============================================================================

        /**
         * The translation, turn and scale that rebuild this transform with FromTranslationRotationScale.
         * A shear (a non-uniform scale under a turn) has no such parts and is lost: the x axis keeps
         * its direction and length, the y scale keeps the area. A flip shows as a negative y scale.
         */
        Affine2DParts Decompose() const noexcept
        {
            Affine2DParts lParts;
            lParts.Translation = Translation;

            const float lScaleX = Maths::Sqrt(XAxis.x * XAxis.x + XAxis.y * XAxis.y);
            if (Maths::IsNearlyZero(lScaleX))
            {
                lParts.Scale = { 0.f, Maths::Sqrt(YAxis.x * YAxis.x + YAxis.y * YAxis.y) };
                return lParts;
            }

            lParts.Rotation = Maths::RadiansToDegrees(Maths::Atan2(XAxis.y, XAxis.x));
            lParts.Scale    = { lScaleX, Determinant() / lScaleX };
            return lParts;
        }

        bool IsIdentity() const noexcept
        {
            return XAxis.x == 1.f && XAxis.y == 0.f && YAxis.x == 0.f && YAxis.y == 1.f
                && Translation.x == 0.f && Translation.y == 0.f;
        }
    };
}
