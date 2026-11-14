#pragma once

#include "Core/Log/Logger.h"
#include "RHI/IGraphicsContext.h"

namespace Opaax
{
    inline constexpr LogCategory LogOpenGLContext{"OpenGLContext"};
    
    // =============================================================================
    // OpenGLContext
    // =============================================================================
    /**
     * IGraphicsContext for OpenGL on a window's surface: make current, glad loading, vsync, swap.
     */
    class OpenGLContext final : public IGraphicsContext
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /** A context on InSurface, which outlives it. Nothing is done before Init. */
        explicit OpenGLContext(IGLSurface& InSurface);

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IGraphicsContext interface
    public:
        bool Init()                   override;
        void SwapBuffers()            override;
        void SetSwapInterval(Uint32 InRefreshes) override;
        //~End IGraphicsContext interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        // Logs adapter and driver details at init.
        void LogAdapterInfo() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IGLSurface* m_Surface = nullptr;
    };

} // namespace Opaax
