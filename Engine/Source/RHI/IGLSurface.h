#pragma once

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // GLSurfaceDesc — what an OpenGL context asks of its window before the window is made: the
    //   version, the profile, a debug context. The platform's window turns it into its own hints.
    // =============================================================================
    struct GLSurfaceDesc
    {
        Uint32 MajorVersion = 4;
        Uint32 MinorVersion = 1;

        /** The version's own functions, without those older versions removed: macOS gives above 3.2 only this way. */
        bool bCoreProfile = true;

        /** No function marked old is offered: macOS asks for it with a core profile. */
        bool bForwardCompatible = true;

        /** The driver says which call is wrong (OpenGLDebug). Costs some speed. */
        bool bDebugContext = false;
    };

    /** Finds an OpenGL function by name in the current context; null when the driver has none. */
    using GLProcLoader = void* (*)(const char* InName);

    // =============================================================================
    // IGLSurface — what a window gives an OpenGL context. Written by the platform's window
    //   (Platform/Glfw/GlfwGLSurface), so the RHI never sees the window's library.
    // =============================================================================
    class IGLSurface
    {
    public:
        virtual ~IGLSurface() = default;

        /** Makes the window's context the current one on this thread. */
        virtual void MakeCurrent() = 0;

        /** The function that finds OpenGL functions; what it finds belongs to the current context. */
        virtual GLProcLoader GetProcLoader() const = 0;

        /** Shows the frame drawn into the back buffer. */
        virtual void SwapBuffers() = 0;

        /** The refreshes of the screen each frame is shown for: 1 is vsync, 0 none. */
        virtual void SetSwapInterval(Uint32 InRefreshes) = 0;
    };
}
