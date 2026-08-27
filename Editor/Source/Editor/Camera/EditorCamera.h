#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class World;

    OPAAX_LOG_CATEGORY(EditorCamera);
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorCamera — how the AUTHOR is looking at an Edit world. The Edit-side producer of
    //   World::CameraView, opposite CameraManager's Play-side one; neither knows the other
    //   exists, because the world's view slot is all they share (D4).
    //
    //   ONE instance for the editor's whole life, owned by EditorService and reached through
    //   EditorContext. That is not a convenience — it is what makes pan and zoom survive a PIE
    //   cycle: Play swaps the active world, this object is untouched, and Stop finds it exactly
    //   where it was left. Legacy hung its editor camera off EditorSubsystem for the same reason.
    //
    //   It speaks the SAME vocabulary as CameraComponent — a position and an OrthoSize in world
    //   units — so "zoom" here is nothing but a smaller vertical half-extent, and the editor can
    //   never frame a world in a way the game could not.
    //
    //   IT IS DRIVEN FROM IMGUI, and that is forced rather than preferred (IN8): with an Edit
    //   world on screen the input route is ClosedEditMode, so InputManager is never fed and would
    //   report every button as up, forever. ViewportPanel measures the gesture while its window is
    //   current and calls in here — the same source Ctrl+S already uses.
    // =============================================================================
    class EditorCamera
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Grab-the-world pan, in SCREEN pixels: drag right and the content follows the cursor
         * right, which means the camera moves left. Y is flipped once — screen-Y grows down,
         * world-Y grows up.
         *
         * The viewport size is needed because a pixel is only worth a fixed number of world units
         * at a given OrthoSize; zoomed in, the same drag covers less world.
         */
        void Pan(const Vector2F& InScreenDelta, const Vector2F& InViewportPx);

        /**
         * Scroll-wheel zoom ANCHORED at the cursor: the world point under the pointer is still
         * under the pointer afterwards. Positive InWheel zooms in (a smaller OrthoSize).
         *
         * Read the world point, change the size, read it again, translate by the difference —
         * salvaged from the legacy editor camera, which is where this was already right.
         */
        void ZoomAtCursor(float InWheel, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx);

        /**
         * Publish this camera as InWorld's view — but ONLY for an Edit world. A Play world is
         * framed by its own CameraComponent, so this refuses rather than fighting CameraManager
         * for the slot. THE ONE PLACE the Edit/Play fork is stated on the editor's side.
         */
        void Apply(World& InWorld) const;

        /**
         * Adopt the viewport's height as the starting OrthoSize, ONCE, the first time a real
         * height exists.
         *
         * This is what makes the editor open on the framing it opened on before cameras existed —
         * that view was one world unit per pixel, so half the panel's height IS the equivalent
         * OrthoSize. Ignores the 1x1 the panel reports before its first measured resize.
         */
        void SeedFromViewportHeight(float InHeightPx);

    private:
        /** One Info the first time this camera actually moves — see m_bMovedLogged. */
        void LogFirstMove(const char* InGesture);

        // =============================================================================
        // Get - Set
        // =============================================================================
    public:
        const Vector2F& GetPosition() const noexcept  { return m_Position; }
        float           GetOrthoSize() const noexcept { return m_OrthoSize; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2F m_Position  = { 0.f, 0.f };
        float    m_OrthoSize = 300.f;

        bool     m_bSeeded   = false;

        // One-shot: the first pan or zoom says so. Two silent frames of "nothing moved" are
        // otherwise indistinguishable between a gesture that never arrived and a camera that never
        // reached the world, and only one of those is fixable from a log.
        bool     m_bMovedLogged = false;
    };
}
