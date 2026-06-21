#pragma once

#include "ICamera.h"

namespace Opaax
{
    /**
     * @class OrthographicCamera
     *
     * Y-up 2D orthographic camera. Origin (0,0) at screen centre by default.
     * View/Projection are recomputed lazily — mark dirty on position/zoom/viewport change,
     * recompute on next GetViewProjection() call.
     *
     * No engine-app coupling: this is pure data + matrices. Lifetime is owned by
     * RenderSubsystem (or EditorSubsystem for the editor camera).
     */
    class OPAAX_API OrthographicCamera : public ICamera
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        OrthographicCamera()           = default;
        ~OrthographicCamera() override = default;

        OrthographicCamera(const OrthographicCamera&)            = delete;
        OrthographicCamera& operator=(const OrthographicCamera&) = delete;
        OrthographicCamera(OrthographicCamera&&)                 = delete;
        OrthographicCamera& operator=(OrthographicCamera&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void RecalculateViewProjection();

        //------------------------------------------------------------------------------
        //  Get - Set
    public:
        void SetZoom(float InZoom);

        FORCEINLINE float GetZoom() const noexcept { return m_Zoom; }

        /**
         * Convert a viewport-local pixel coordinate (origin at panel top-left, Y growing
         * downward) into a world-space coordinate. Used by the editor for zoom-at-cursor:
         * snapshot the world point under the cursor pre-zoom, change the zoom, snapshot
         * again, then translate the camera so the post-zoom world point coincides with
         * the pre-zoom one.
         */
        FORCEINLINE Vector2F ScreenToWorld(const Vector2F& InLocalPx, const Vector2F& InViewportPx) const noexcept
        {
            // Centred ortho: viewport-local (0,0) maps to (-w/2, +h/2) in world; screen-Y-down
            // vs world-Y-up forces the Y flip on the centred coord. Effective camera position
            // includes m_TransientOffset because that's what the view matrix actually translates by.
            const Vector2F lCentred   = InLocalPx - InViewportPx * 0.5f;
            const Vector2F lEffective = m_Position + m_TransientOffset;
            return lEffective + Vector2F(lCentred.x, -lCentred.y) / m_Zoom;
        }

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICamera Interface
    public:
        const Matrix44F& GetViewProjection() override;
        void             SetViewportSize(Uint32 InWidth, Uint32 InHeight) override;

        Vector2F GetPosition() const override { return m_Position; }
        void     SetPosition(const Vector2F& InPosition) override;
        void     AddPositionOffset(const Vector2F& InOffset) override;
        void     ResetTransientOffsets() override;
        //~End ICamera Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2F  m_Position        = { 0.f, 0.f };
        // Transient per-frame offset written by modifier controllers (Shake). Cleared
        // at the start of every CameraControllerSystem::Update before controllers tick,
        // so it never leaks into m_Position and never bleeds into Follow's lerp source.
        Vector2F  m_TransientOffset = { 0.f, 0.f };
        float     m_Zoom            = 1.f;

        Uint32    m_ViewportWidth  = 0;
        Uint32    m_ViewportHeight = 0;

        Matrix44F m_ProjectionMatrix     = Matrix44F(1.f);
        Matrix44F m_ViewMatrix           = Matrix44F(1.f);
        Matrix44F m_ViewProjectionMatrix = Matrix44F(1.f);

        bool      m_bDirty = true;
    };

} // namespace Opaax
