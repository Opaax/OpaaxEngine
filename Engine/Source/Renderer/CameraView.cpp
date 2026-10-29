#include "Renderer/CameraView.h"

#include <glm/gtc/matrix_transform.hpp>

#include "Core/Maths/Maths.h"

namespace Opaax
{
    namespace
    {
        /** InVector turned by InDegrees, counter-clockwise. Unturned, it is given back as it is. */
        Vector2F Turn(const Vector2F& InVector, const float InDegrees)
        {
            if (InDegrees == 0.f)
            {
                return InVector;
            }

            const float lRad = Maths::DegreesToRadians(InDegrees);
            const float lCos = Maths::Cos(lRad);
            const float lSin = Maths::Sin(lRad);
            return { lCos * InVector.x - lSin * InVector.y, lSin * InVector.x + lCos * InVector.y };
        }
    }

    Matrix44F MakeView(const CameraView& InView)
    {
        // Translate by -Position: the world moves opposite to the camera, and turns opposite to its roll.
        const Matrix44F lTranslate = glm::translate(Matrix44F(1.f), glm::vec3(-InView.Position.x, -InView.Position.y, 0.f));
        if (InView.Rotation == 0.f)
        {
            return lTranslate;
        }

        return glm::rotate(Matrix44F(1.f), -Maths::DegreesToRadians(InView.Rotation), glm::vec3(0.f, 0.f, 1.f)) * lTranslate;
    }

    Matrix44F MakeProjection(const CameraView& InView, Uint32 InWidth, Uint32 InHeight)
    {
        if (InWidth == 0 || InHeight == 0)
        {
            return Matrix44F(1.f);
        }

        const float lHalfH = InView.OrthoSize;
        const float lHalfW = lHalfH * (static_cast<float>(InWidth) / static_cast<float>(InHeight));

        return glm::ortho(-lHalfW, lHalfW, -lHalfH, lHalfH, -1.f, 1.f);
    }

    Matrix44F MakeViewProjection(const CameraView& InView, Uint32 InWidth, Uint32 InHeight)
    {
        // Zero-size check here too: identity * view is the view, not identity.
        if (InWidth == 0 || InHeight == 0)
        {
            return Matrix44F(1.f);
        }

        return MakeProjection(InView, InWidth, InHeight) * MakeView(InView);
    }

    Vector2F ScreenToWorld(const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InLocalPx)
    {
        if (InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return InView.Position;
        }

        const float lHalfH = InView.OrthoSize;
        const float lHalfW = lHalfH * (InViewportPx.x / InViewportPx.y);

        // Centre-relative pixels scaled to world units, then turned by the roll. Y is flipped (screen
        // down, world up).
        const Vector2F lCentred = InLocalPx - InViewportPx * 0.5f;

        return InView.Position + Turn(Vector2F(lCentred.x / (InViewportPx.x * 0.5f) * lHalfW,
                                               -lCentred.y / (InViewportPx.y * 0.5f) * lHalfH),
                                      InView.Rotation);
    }

    Vector2F WorldToScreen(const CameraView& InView, const Vector2F& InViewportPx, const Vector2F& InWorld)
    {
        if (InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return InViewportPx * 0.5f;
        }

        const float lHalfH = InView.OrthoSize;
        const float lHalfW = lHalfH * (InViewportPx.x / InViewportPx.y);

        const Vector2F lCentred = Turn(InWorld - InView.Position, -InView.Rotation);

        return InViewportPx * 0.5f + Vector2F(lCentred.x / lHalfW * (InViewportPx.x * 0.5f),
                                              -lCentred.y / lHalfH * (InViewportPx.y * 0.5f));
    }

    float WorldPerPixel(const CameraView& InView, const float InViewportHeightPx)
    {
        return InViewportHeightPx > 0.f ? (InView.OrthoSize * 2.f) / InViewportHeightPx : 1.f;
    }

    Bounds2D ViewBounds(const CameraView& InView, const float InAspect)
    {
        const Vector2F lSize{ 2.f * InView.OrthoSize * InAspect, 2.f * InView.OrthoSize };
        if (InView.Rotation == 0.f)
        {
            return Bounds2D::FromCenterSize(InView.Position, lSize);
        }

        return Bounds2D::FromCenterSizeRotated(InView.Position, lSize, Maths::DegreesToRadians(InView.Rotation));
    }

    CameraView ZoomAround(const CameraView& InView, const float InOrthoSize, const Vector2F& InViewportPx,
                          const Vector2F& InAnchorPx)
    {
        CameraView lZoomed = InView;
        lZoomed.OrthoSize  = InOrthoSize;

        // The anchor's world point before and after the size changes; moving by the difference puts
        // it back under the anchor.
        const Vector2F lBefore = ScreenToWorld(InView, InViewportPx, InAnchorPx);
        const Vector2F lAfter  = ScreenToWorld(lZoomed, InViewportPx, InAnchorPx);
        lZoomed.Position += lBefore - lAfter;

        return lZoomed;
    }
}
