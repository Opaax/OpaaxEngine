#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class IGraphicsContext;
    // Forward-declared (used only by reference in the callback signature) so Window.h
    // does not drag the event-types header into every consumer.
    class Event;

    using EventCallbackFunc = TFunction<void(Event&)>;
    
    enum class EWindowMode
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

        WindowProps(const OpaaxString& Title = "Opaax Engine",
            Uint32 Width = 1280,
            Uint32 Height = 720,
            EWindowMode Mode = EWindowMode::Windowed)
            : Title(Title), Width(Width), Height(Height), Mode(Mode)
        {
        }

        // =============================================================================
        // Members
        // =============================================================================

        OpaaxString Title;
        Uint32 Width;
        Uint32 Height;
        EWindowMode Mode;
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

        /**
         * Ask the window to close — the programmatic equivalent of clicking its X (M5).
         *
         * Deliberately routed through the window rather than through an "application, stop"
         * call: the close flag is what RunApplication already polls (bIsRunning =
         * !ShouldClose()) and what raises WindowCloseEvent, so an editor menu's Exit takes the
         * SAME path a user's click takes. A second way to stop the loop would be a second thing
         * to keep correct.
         */
        virtual void RequestClose()         = 0;
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
        
        virtual EWindowMode GetWindowMode() const           = 0;
        virtual void        SetWindowMode(EWindowMode mode) = 0;
        
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
