#pragma once

#include "Core/Systems/EngineSubsystem.h"
#include "Core/ApplicationEvents.hpp"
 
#include <glm/glm.hpp>

#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    /**
     * @class Camera2D
     * Orthographic camera. Registered as an engine subsystem so it:
     *  - Automatically recomputes projection on WindowResizeEvent
     *  - Is accessible from anywhere via GetSubsystem<Camera2D>()
     *
     *  Coordinate system: Y-up. Origin (0,0) at screen centre by default.
     *
     *  The ViewProjection matrix is recomputed only when position or zoom changes —
     *  not every frame. Mark dirty, recompute on next GetViewProjection() call.
     * 
     */
    class OPAAX_API Camera2D final : public EngineSubsystemBase
    {
    public:
        OPAAX_SUBSYSTEM_TYPE(Camera2D)
        
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        Camera2D() = default;
        explicit Camera2D(CoreEngineApp* InEngineApp) : EngineSubsystemBase(InEngineApp) {}
        ~Camera2D() override = default;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        Camera2D(const Camera2D&)            = delete;
        Camera2D& operator=(const Camera2D&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
        Camera2D(Camera2D&&)                 = default;
        Camera2D& operator=(Camera2D&&)      = default;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void RecalculateViewProjection();
        void SetViewportSize(Uint32 InWidth, Uint32 InHeight);
        bool OnWindowResize(WindowResizeEvent& Event);

        //------------------------------------------------------------------------------
        //  Get - Set
    public:
        void SetPosition(const Vector2F& InPosition);
        void SetZoom    (float InZoom);
 
        FORCEINLINE const Vector2F&     GetPosition()   const noexcept { return m_Position; }
        FORCEINLINE float               GetZoom()       const noexcept { return m_Zoom; }
 
        // Returns the combined ViewProjection matrix.
        // Recomputes only if dirty.
        const Matrix44F& GetViewProjection();
 
        

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin EngineSubsystemBase Interface
    public:
        bool Startup()  override;
        void Shutdown() override;
 
        Uint32 GetEventCategoryFilter() const noexcept override
        {
            return EEventCategory_Application;
        }
 
        bool OnEvent(OpaaxEvent& Event) override;
        //~End EngineSubsystemBase Interface
 
        // =============================================================================
        // Members
        // =============================================================================
    private:
        Vector2F    m_Position = { 0.f, 0.f };
        float       m_Zoom     = 1.f;
 
        Uint32      m_ViewportWidth  = 1280;
        Uint32      m_ViewportHeight = 720;
 
        Matrix44F   m_ProjectionMatrix     = Matrix44F(1.f);
        Matrix44F   m_ViewMatrix           = Matrix44F(1.f);
        Matrix44F   m_ViewProjectionMatrix = Matrix44F(1.f);
 
        bool        m_bDirty = true;
    };
 
} // namespace Opaax
