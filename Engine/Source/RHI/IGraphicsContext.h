#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/RenderAPI.h"

namespace Opaax
{
    // =============================================================================
    // IGraphicsContext
    // =============================================================================
    /**
     * @interface IGraphicsContext
     *
     * Owns the graphics-context side of the window: making the context current,
     * loading the backend's function pointers, vsync, and presenting (swap).
     * The window creates the OS/GLFW window; the context turns it into a render
     * surface for the selected backend. This is the one sanctioned place a backend
     * touches platform graphics bootstrap (glad for OpenGL; a swapchain for Vulkan).
     *
     * The concrete impl is selected by IGraphicsContext::Create, defined in the
     * active backend's TU (OpenGLContext.cpp today) — mirrors the RenderAPI::Create
     * and IShader::Create factory pattern.
     */
    class OPAAX_API IGraphicsContext
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
         * Build the context for a backend over an already-created native window.
         * @param InBackend      selected graphics backend
         * @param InNativeWindow opaque native window handle (GLFWwindow* today)
         * @return owning context, or nullptr on unknown backend (logged)
         */
        static UniquePtr<IGraphicsContext> Create(EBackend InBackend, void* InNativeWindow);

        /**
         * Apply backend-specific GLFW window hints. MUST run before glfwCreateWindow.
         * OpenGL: no-op (driver default, behavior-preserving). Vulkan: GLFW_NO_API.
         * @param InBackend selected graphics backend
         */
        static void ApplyWindowHints(EBackend InBackend);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Make the context current + load the backend's function pointers + set vsync.
         * @return false if context bring-up failed (e.g. glad could not load).
         */
        virtual bool Init() = 0;

        // Present the rendered frame to the window surface.
        virtual void SwapBuffers() = 0;

        // Enable/disable vertical sync.
        virtual void SetVSync(bool InEnabled) = 0;
    };

} // namespace Opaax
