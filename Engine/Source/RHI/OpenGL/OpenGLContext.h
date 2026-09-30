#pragma once

#include "Core/Log/Logger.h"
#include "RHI/IGraphicsContext.h"

struct GLFWwindow;

namespace Opaax
{
    inline constexpr LogCategory LogOpenGLContext{"OpenGLContext"};
    
    // =============================================================================
    // OpenGLContext
    // =============================================================================
    /**
     * IGraphicsContext for OpenGL on a GLFW window: make current, glad loading, vsync, swap.
     */
    class OPAAX_API OpenGLContext final : public IGraphicsContext
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLContext(GLFWwindow* InWindow);

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IGraphicsContext interface
    public:
        bool Init()                   override;
        void SwapBuffers()            override;
        void SetVSync(bool InEnabled) override;
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
        GLFWwindow* m_Window = nullptr;
    };

} // namespace Opaax
