#include "CameraView.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Opaax
{
    Matrix44F MakeView(const CameraView& InView)
    {
        // Translate by -Position: the world moves opposite to the camera.
        return glm::translate(Matrix44F(1.f), glm::vec3(-InView.Position.x, -InView.Position.y, 0.f));
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
        // The zero-size guard stays HERE as well as in MakeProjection: identity * view is the view,
        // not identity, so deferring to the half would quietly change what a degenerate target
        // answers for any camera that is not at the origin.
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

        // Centre-relative pixels, then scaled to world half-extents. Y is negated once:
        // screen-Y grows down, world-Y grows up.
        const Vector2F lCentred = InLocalPx - InViewportPx * 0.5f;

        return InView.Position + Vector2F(lCentred.x / (InViewportPx.x * 0.5f) * lHalfW,
                                          -lCentred.y / (InViewportPx.y * 0.5f) * lHalfH);
    }
}
