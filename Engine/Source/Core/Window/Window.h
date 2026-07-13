#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxForward.h"
#include "Core/EventOld/OpaaxEvent.hpp"

namespace Opaax
{
    class IGraphicsContext;

    using EventCallbackFunc = TFunction<void(OpaaxEvent&)>;
    
    enum class WindowMode
    {
        Windowed,
        Borderless,
        Fullscreen
    };

    
    /**
     * @struct WindowProps
     * 
     */
    struct WindowProps
    {
        // =============================================================================
        // CTOR
        // =============================================================================
        
        WindowProps(const String& Title = "Opaax Engine",
            Uint32 Width = 1280,
            Uint32 Height = 720)
            : Title(Title), Width(Width), Height(Height), WindowMode(WindowMode::Windowed)
        {
        }

        // =============================================================================
        // Members
        // =============================================================================
        
        String Title;
        Uint32 Width;
        Uint32 Height;
        WindowMode WindowMode;
    };
    
    
    /**
     * 
     */
    class OPAAX_API Window
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        static Window* Create(const WindowProps& props = WindowProps());

        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        Window() = default;
        virtual ~Window() = default;

        // Delete Copy and Move Operations
        Window(const Window&) = delete;               // Copy Constructor
        Window& operator=(const Window&) = delete;    // Copy Assignment Operator
        Window(Window&&) = delete;                    // Move Constructor
        Window& operator=(Window&&) = delete;         // Move Assignment Operator

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void PollEvents()           = 0;
        virtual void SwapBuffers()          = 0;
        virtual bool ShouldClose() const    = 0;
        virtual void Shutdown()             = 0;

        // =============================================================================
        // Get - Set
    public:
        virtual void SetEventCallback(const EventCallbackFunc& Callback)    = 0;
        
        virtual void*   GetNativeWindow()   const = 0;
        virtual Uint32  GetWidth()          const = 0;
        virtual Uint32  GetHeight()         const = 0;
        
        /*-------------------------------------------------------------------------*/
        // Window Mode
        
        virtual WindowMode  GetWindowMode() const           = 0;
        virtual void        SetWindowMode(WindowMode mode)  = 0;
        
        virtual void SetWindowed()          = 0;
        virtual void SetBorderless()        = 0;
        virtual void SetFullscreen()        = 0;
        
        virtual void SaveWindowedState()    = 0;
        
        // Window Mode 
        /*-------------------------------------------------------------------------*/

        /**
         * @return The backend graphics context the window owns.
         * A command-buffer backend (Vulkan) borrows the shared device/swapchain from it; the OpenGL render API ignores it.
         */
        virtual IGraphicsContext* GetGraphicsContext() const = 0;
        
        // Get - Set
        // =============================================================================
    };
}
