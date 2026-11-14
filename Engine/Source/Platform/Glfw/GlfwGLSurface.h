#pragma once

#include "RHI/IGLSurface.h"

struct GLFWwindow;

namespace Opaax
{
    // =============================================================================
    // GlfwGLSurface — a GLFW window's OpenGL context, as the RHI uses it (IGLSurface): GLFW stays in
    //   Platform/, the RHI only knows the surface.
    // =============================================================================
    class GlfwGLSurface final : public IGLSurface
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        /** Sets GLFW's hints for the next window made, from what the context asks. */
        static void ApplyHints(const GLSurfaceDesc& InDesc);

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        /** The context of InWindow, which outlives the surface. */
        explicit GlfwGLSurface(GLFWwindow* InWindow);

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IGLSurface interface
    public:
        void         MakeCurrent() override;
        GLProcLoader GetProcLoader() const override;
        void         SwapBuffers() override;
        void         SetSwapInterval(Uint32 InRefreshes) override;
        //~End IGLSurface interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window = nullptr;
    };
}
