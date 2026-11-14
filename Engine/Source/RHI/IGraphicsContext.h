#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/IGLSurface.h"
#include "RHI/RHIBackend.h"

namespace Opaax
{
    // =============================================================================
    // IGraphicsContext
    // =============================================================================
    /**
     * The graphics context of a window: make current, load function pointers, vsync, present.
     * Created by IGraphicsContext::Create (BackendFactory.cpp). OpenGL is the only backend.
     */
    class IGraphicsContext
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IGraphicsContext() = default;

        // =============================================================================
        // Statics
        // =============================================================================

        /**
         * Creates the context for a backend on a window's surface, which outlives it.
         * @return The context, or nullptr for an unknown backend (logged)
         */
        static TUniquePtr<IGraphicsContext> Create(EBackend InBackend, IGLSurface& InSurface);

        /**
         * What the backend's context asks of the window, given to the window before it is made.
         * OpenGL: version 4.1, core profile, forward compatible, and a debug context in a dev build.
         */
        static GLSurfaceDesc DescribeSurface(EBackend InBackend);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Makes the context current, loads the function pointers and sets vsync.
         * @return False if it failed (e.g. glad could not load)
         */
        virtual bool Init() = 0;

        // Presents the frame to the window.
        virtual void SwapBuffers() = 0;

        /**
         * The refreshes of the screen each frame is shown for: 1 is vsync, 2 every other refresh (a frame
         * cap that divides the screen's rate), 0 no vsync. A driver may show them for one only.
         */
        virtual void SetSwapInterval(Uint32 InRefreshes) = 0;
    };

} // namespace Opaax
