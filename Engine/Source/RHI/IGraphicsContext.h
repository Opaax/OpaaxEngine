#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/RHIBackend.h"

namespace Opaax
{
    // =============================================================================
    // IGraphicsContext
    // =============================================================================
    /**
     * The graphics context of a window: make current, load function pointers, vsync, present.
     * Created by IGraphicsContext::Create (BackendFactory.cpp). OpenGL only for now.
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
         * Creates the context for a backend on an existing native window.
         * @param InBackend      Graphics backend
         * @param InNativeWindow Native window handle (GLFWwindow*)
         * @return The context, or nullptr for an unknown backend (logged)
         */
        static TUniquePtr<IGraphicsContext> Create(EBackend InBackend, void* InNativeWindow);

        /**
         * Sets backend-specific GLFW window hints. Call before glfwCreateWindow.
         * OpenGL: nothing. Vulkan: GLFW_NO_API.
         */
        static void ApplyWindowHints(EBackend InBackend);

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

        // Enables or disables vsync.
        virtual void SetVSync(bool InEnabled) = 0;
    };

} // namespace Opaax
