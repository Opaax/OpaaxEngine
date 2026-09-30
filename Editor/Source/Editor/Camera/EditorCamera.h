#pragma once

#include "Core/Log/Logger.h"
#include "Core/Maths/Bounds2D.h"
#include "Core/Maths/MathTypes.h"

namespace Opaax
{
    class World;

    OPAAX_LOG_CATEGORY(EditorCamera);
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorCamera — how the author looks at an Edit world. Produces World::CameraView for Edit
    //   worlds (CameraManager does it for Play worlds).
    //   One instance for the editor's lifetime, owned by EditorService, so pan and zoom survive a
    //   Play/Stop cycle. Uses the same terms as CameraComponent (position, OrthoSize).
    //   Driven from ImGui: in Edit mode the InputManager is not fed, so ViewportPanel measures the
    //   gesture and calls in here.
    // =============================================================================
    class EditorCamera
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Pans by a drag in screen pixels: the content follows the cursor. Y is flipped (screen Y goes
         * down, world Y goes up). The viewport size converts pixels to world units.
         */
        void Pan(const Vector2F& InScreenDelta, const Vector2F& InViewportPx);

        /**
         * Zooms around the cursor: the world point under the pointer stays under it. Positive InWheel
         * zooms in (smaller OrthoSize).
         */
        void ZoomAtCursor(float InWheel, const Vector2F& InCursorLocalPx, const Vector2F& InViewportPx);

        /**
         * Frames InBounds: centres on it and zooms so it fits both axes, with a margin. A zero-size
         * target keeps a minimum size.
         */
        void FocusOn(const Bounds2D& InBounds, const Vector2F& InViewportPx);

        /**
         * Puts the camera exactly here. Counts as seeded, so the first-resize seed does not overwrite it.
         */
        void Set(const Vector2F& InPosition, float InOrthoSize) noexcept;

        /**
         * Sets this camera as InWorld's view, for Edit worlds only (a Play world uses its CameraComponent).
         */
        void Apply(World& InWorld) const;

        /**
         * Takes the viewport's height as the starting OrthoSize, once, when a real height is first known
         * (one world unit per pixel). Ignores the 1x1 reported before the first resize.
         */
        void SeedFromViewportHeight(float InHeightPx);

    private:
        /** Logs once, the first time this camera moves. */
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

        // Logs the first pan or zoom once (tells "no gesture" apart from "camera not applied").
        bool     m_bMovedLogged = false;
    };
}
