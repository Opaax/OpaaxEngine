#include "Editor/Camera/EditorCamera.h"

#include "Core/Maths/Maths.h"
#include "Renderer/CameraView.h"
#include "World/World.h"

namespace Opaax::Editor
{
    namespace
    {
        constexpr float k_ZoomStep    = 1.1f;
        constexpr float k_OrthoSizeMin = 1.f;      // one world unit fills half the viewport
        constexpr float k_OrthoSizeMax = 50000.f;

        // Focus leaves the target filling about 80% of the height, and never zooms closer than the minimum.
        constexpr float k_FocusMargin       = 1.25f;
        constexpr float k_FocusMinOrthoSize = 100.f;
    }

    void EditorCamera::Pan(const Vector2F& InScreenDelta, const Vector2F& InViewportPx)
    {
        if ((InScreenDelta.x == 0.f && InScreenDelta.y == 0.f) || InViewportPx.y <= 0.f)
        {
            return;
        }

        const float lScale = WorldPerPixel(CameraView{ m_Position, m_OrthoSize }, InViewportPx.y);

        m_Position += Vector2F(-InScreenDelta.x, InScreenDelta.y) * lScale;

        LogFirstMove("pan");
    }

    void EditorCamera::ZoomAtCursor(float InWheel, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx)
    {
        if (InWheel == 0.f || InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return;
        }

        // Wheel up zooms in: a smaller half-extent, hence the negated exponent.
        const float lNewSize = Maths::Clamp(m_OrthoSize * Maths::Pow(k_ZoomStep, -InWheel),
                                            k_OrthoSizeMin, k_OrthoSizeMax);
        if (lNewSize == m_OrthoSize)
        {
            return;
        }

        const Vector2F lBefore = ScreenToWorld(CameraView{ m_Position, m_OrthoSize }, InViewportPx, InCursorLocalPx);

        m_OrthoSize = lNewSize;

        const Vector2F lAfter = ScreenToWorld(CameraView{ m_Position, m_OrthoSize }, InViewportPx, InCursorLocalPx);

        m_Position += lBefore - lAfter;

        LogFirstMove("zoom");
    }

    void EditorCamera::FocusOn(const Bounds2D& InBounds, const Vector2F& InViewportPx)
    {
        if (InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return;
        }

        m_Position = InBounds.Center;

        // Fit both axes: OrthoSize is the vertical half-extent and the width follows the aspect.
        const float lAspect = InViewportPx.x / InViewportPx.y;
        const float lNeeded = Maths::Max(InBounds.HalfExtent.y, InBounds.HalfExtent.x / lAspect);

        // The margin keeps the target off the edge; the floor stops a zero-size target collapsing the view.
        m_OrthoSize = Maths::Clamp(Maths::Max(lNeeded * k_FocusMargin, k_FocusMinOrthoSize),
                                   k_OrthoSizeMin, k_OrthoSizeMax);

        // Focus jumps somewhere the author could not see, so it is logged every time.
        OPAAX_LOG(LogEditorCamera, Info, "Editor camera focused on ({}, {}) — orthoSize {}",
                  m_Position.x, m_Position.y, m_OrthoSize);

        m_bSeeded      = true;   // an explicit framing is not overwritten by the seed
        m_bMovedLogged = true;
    }

    void EditorCamera::Set(const Vector2F& InPosition, const float InOrthoSize) noexcept
    {
        m_Position  = InPosition;
        m_OrthoSize = Maths::Clamp(InOrthoSize, k_OrthoSizeMin, k_OrthoSizeMax);
        m_bSeeded   = true;
    }

    void EditorCamera::Apply(World& InWorld) const
    {
        if (InWorld.GetMode() != EWorldMode::Edit)
        {
            return;
        }

        InWorld.SetCameraView(CameraView{ m_Position, m_OrthoSize });
    }

    void EditorCamera::SeedFromViewportHeight(float InHeightPx)
    {
        // > 1, not > 0: the panel reports 1x1 until its first real resize.
        if (m_bSeeded || InHeightPx <= 1.f)
        {
            return;
        }

        m_OrthoSize = InHeightPx * 0.5f;
        m_bSeeded   = true;
    }

    void EditorCamera::LogFirstMove(const char* InGesture)
    {
        if (m_bMovedLogged)
        {
            return;
        }

        m_bMovedLogged = true;

        OPAAX_LOG(LogEditorCamera, Trace, "Editor camera moved ({}) — position ({}, {}), orthoSize {}",
                  InGesture, m_Position.x, m_Position.y, m_OrthoSize);
    }
}
