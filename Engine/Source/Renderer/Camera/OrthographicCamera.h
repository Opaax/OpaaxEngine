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
     * CameraSubsystem (or EditorSubsystem for the editor camera).
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

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICamera Interface
    public:
        const Matrix44F& GetViewProjection() override;
        void             SetViewportSize(Uint32 InWidth, Uint32 InHeight) override;

        Vector2F GetPosition() const override { return m_Position; }
        void     SetPosition(const Vector2F& InPosition) override;
        //~End ICamera Interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2F  m_Position = { 0.f, 0.f };
        float     m_Zoom     = 1.f;

        Uint32    m_ViewportWidth  = 0;
        Uint32    m_ViewportHeight = 0;

        Matrix44F m_ProjectionMatrix     = Matrix44F(1.f);
        Matrix44F m_ViewMatrix           = Matrix44F(1.f);
        Matrix44F m_ViewProjectionMatrix = Matrix44F(1.f);

        bool      m_bDirty = true;
    };

} // namespace Opaax
