#pragma once

#include "EngineAPI.h"
#include "OpaaxTypes.h"
#include "OpaaxForward.h"
#include "Event/OpaaxEvent.hpp"

namespace Opaax
{
    using EventCallbackFunc = TFunction<void(OpaaxEvent&)>;
    
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
            : Title(Title), Width(Width), Height(Height)
        {
        }

        // =============================================================================
        // Members
        // =============================================================================
        
        String Title;
        Uint32 Width;
        Uint32 Height;
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

        /*----------------------------- Get - Set -------------------------------*/

        virtual void SetEventCallback(const EventCallbackFunc& Callback) = 0;
        
        virtual void* GetNativeWindow() const = 0;
        virtual Uint32 GetWidth()       const = 0;
        virtual Uint32 GetHeight()      const = 0;
    };
}
