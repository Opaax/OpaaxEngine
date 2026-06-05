#pragma once

#include "RHI/IGraphicsContext.h"

struct GLFWwindow;

namespace Opaax
{
    // =============================================================================
    // OpenGLContext
    // =============================================================================
    /**
     * @class OpenGLContext
     *
     * IGraphicsContext for OpenGL over a GLFW window. Owns make-current, the glad
     * function-pointer load, vsync, and the buffer swap. glad/GLFW are confined to
     * this TU on the OpenGL side — nothing above RHI/ touches them.
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
        // Verbose adapter/driver dump at init (renderer, vendor, GLSL, limits).
        void LogAdapterInfo() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window = nullptr;
    };

} // namespace Opaax
