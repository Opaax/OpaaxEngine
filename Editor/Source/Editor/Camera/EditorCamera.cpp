#include "Editor/Camera/EditorCamera.h"

#include "Core/Maths/Maths.h"
#include "Renderer/CameraView.h"
#include "World/World.h"

namespace Opaax::Editor
{
    namespace
    {
        constexpr float k_ZoomStep    = 1.1f;
        constexpr float k_OrthoSizeMin = 1.f;      // a single world unit fills half the viewport
        constexpr float k_OrthoSizeMax = 50000.f;

        // Focus leaves the target filling ~80% of the height rather than touching the edges, and
        // never zooms closer than this — a point-sized entity would otherwise fill the screen.
        constexpr float k_FocusMargin       = 1.25f;
        constexpr float k_FocusMinOrthoSize = 100.f;

        /** World units covered by one viewport pixel. Square, since width follows the aspect. */
        float WorldPerPixel(float InOrthoSize, float InViewportHeightPx)
        {
            return (InOrthoSize * 2.f) / InViewportHeightPx;
        }
    }

    void EditorCamera::Pan(const Vector2F& InScreenDelta, const Vector2F& InViewportPx)
    {
        if ((InScreenDelta.x == 0.f && InScreenDelta.y == 0.f) || InViewportPx.y <= 0.f)
        {
            return;
        }

        const float lScale = WorldPerPixel(m_OrthoSize, InViewportPx.y);

        m_Position += Vector2F(-InScreenDelta.x, InScreenDelta.y) * lScale;

        LogFirstMove("pan");
    }

    void EditorCamera::ZoomAtCursor(float InWheel, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx)
    {
        if (InWheel == 0.f || InViewportPx.x <= 0.f || InViewportPx.y <= 0.f)
        {
            return;
        }

        // Wheel UP zooms IN, which is a SMALLER half-extent — hence the negated exponent.
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

        // Fit BOTH axes. OrthoSize is the vertical half-extent and the width follows the aspect, so
        // a wide selection is fitted by converting its horizontal need into a vertical one.
        const float lAspect = InViewportPx.x / InViewportPx.y;
        const float lNeeded = Maths::Max(InBounds.HalfExtent.y, InBounds.HalfExtent.x / lAspect);

        // The margin keeps the target off the very edge; the floor is what stops a zero-extent
        // target — an entity with only a transform — from collapsing the projection.
        m_OrthoSize = Maths::Clamp(Maths::Max(lNeeded * k_FocusMargin, k_FocusMinOrthoSize),
                                   k_OrthoSizeMin, k_OrthoSizeMax);

        // Focus is a JUMP, and it is the one gesture whose whole point is that the author cannot
        // see the target — so it says where it went every time, not once.
        OPAAX_LOG(LogEditorCamera, Info, "Editor camera focused on ({}, {}) — orthoSize {}",
                  m_Position.x, m_Position.y, m_OrthoSize);

        m_bSeeded      = true;   // an explicit framing must not be overwritten by the one-shot seed
        m_bMovedLogged = true;
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
        // > 1 rather than > 0: the panel reports 1x1 until its first measured resize lands, and
        // seeding off that would open the editor zoomed into half a world unit.
        if (m_bSeeded || InHeightPx <= 1.f)
        {
            return;
        }

        m_OrthoSize = InHeightPx * 0.5f;
        m_bSeeded   = true;

        OPAAX_LOG(LogEditorCamera, Info, "Editor camera seeded from a {}px viewport — orthoSize {}",
                  InHeightPx, m_OrthoSize);
    }

    void EditorCamera::LogFirstMove(const char* InGesture)
    {
        if (m_bMovedLogged)
        {
            return;
        }

        m_bMovedLogged = true;

        OPAAX_LOG(LogEditorCamera, Info, "Editor camera moved ({}) — position ({}, {}), orthoSize {}",
                  InGesture, m_Position.x, m_Position.y, m_OrthoSize);
    }
}
