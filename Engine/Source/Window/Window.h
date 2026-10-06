#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class IGraphicsContext;
    // Forward-declared to keep the event headers out of Window.h.
    class Event;

    using EventCallbackFunc = TFunction<void(Event&)>;
    
    enum class EWindowMode
    {
        Windowed,
        Borderless,
        Fullscreen
    };

    /**
     * Enum to string.
     */
    inline const char* ToString(const EWindowMode InMode) noexcept
    {
        switch (InMode)
        {
        case EWindowMode::Windowed:   return "Windowed";
        case EWindowMode::Borderless: return "Borderless";
        case EWindowMode::Fullscreen: return "Fullscreen";
        }

        return "Windowed";
    }
}

OPAAX_ENUM_VALUES(Opaax::EWindowMode, Windowed, Borderless, Fullscreen)

namespace Opaax
{

    /**
     * Window creation settings.
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
     * Platform-independent window. Create with Window::Create.
     */
    class Window
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
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void PollEvents()           = 0;
        virtual void SwapBuffers()          = 0;
        virtual bool ShouldClose() const    = 0;

        /**
         * Asks the window to close, like clicking its X (same path: WindowCloseEvent fires).
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

        /*-------------------------------------------------------------------------*/
        // Decoration — separate from EWindowMode: whether the OS draws a title bar and borders.
        // A window with a custom title bar is windowed but undecorated.

        virtual void SetDecorated(bool bInDecorated) = 0;
        virtual bool IsDecorated() const             = 0;

        // Decoration
        /*-------------------------------------------------------------------------*/

        /*-------------------------------------------------------------------------*/
        // Placement — used by a custom title bar to move, resize and button the window.

        /**
         * Top-left in screen coordinates. Can be negative (monitor left of the primary, maximized window).
         */
        virtual void GetPosition(Int32& OutX, Int32& OutY) const = 0;
        virtual void SetPosition(Int32 InX, Int32 InY)           = 0;

        virtual void SetSize(Uint32 InWidth, Uint32 InHeight)    = 0;

        virtual void Minimize()          = 0;
        virtual void Maximize()          = 0;
        virtual void Restore()           = 0;
        virtual bool IsMaximized() const = 0;

        // Placement
        /*-------------------------------------------------------------------------*/

        /**
         * @return The window's graphics context
         */
        virtual IGraphicsContext* GetGraphicsContext() const = 0;
        
        // Get - Set
        // =============================================================================
    };
}
