#pragma once

#include "ICamera.h"

namespace Opaax
{
    /**
     * @class ScreenSpaceCamera
     *
     * Fixed pixel-space camera for overlay/HUD draws. Maps 1:1 to the render target:
     * origin (0,0) at the BOTTOM-LEFT, +X right, +Y up, units = pixels. Immune to the
     * world camera's pan/zoom — content drawn through it stays locked to the screen.
     *
     * Y-UP (not the usual UI top-left/Y-down) on purpose: it matches the engine's world
     * convention and M5 Text2D's Y-up layout, so text/sprites render right-side-up here
     * with zero per-draw correction. A future HUD milestone can layer corner-anchor helpers
     * on top. No position/zoom/transient concept — the ICamera position methods stay defaulted.
     */
    class OPAAX_API ScreenSpaceCamera final : public ICamera
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        ScreenSpaceCamera()           = default;
        ~ScreenSpaceCamera() override = default;

        // =============================================================================
        // Function
        // =============================================================================
    private:
        void Recalculate();

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin ICamera interface
    public:
        const Matrix44F& GetViewProjection() override;
        void             SetViewportSize(Uint32 InWidth, Uint32 InHeight) override;
        //~End ICamera interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32    m_Width          = 0;
        Uint32    m_Height         = 0;
        Matrix44F m_ViewProjection = Matrix44F(1.f);
        bool      m_bDirty         = true;
    };
}
