#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxEnum.h"   // OPAAX_ENUM_VALUES — the mode's own value list
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
     * I11's default rule: the mapping lives WITH the enum.
     *
     * It used to sit in IWindowManager.h, for a reason that belonged to its neighbour — an unknown
     * mode had to be loud and Core does not log. That was WindowModeFromString's problem; ToString is
     * total and silent and was only carried along. The parse is generic now (OpaaxEnum.h), so the
     * exception is retired and both halves come home.
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

        /*-------------------------------------------------------------------------*/
        // Decoration — ORTHOGONAL to EWindowMode.
        //
        // Mode answers "which monitor, at what size"; this answers "does the OS draw a caption and
        // a sizing border". Borderless conflates them, so a host that draws its own title bar and
        // is otherwise an ordinary movable window needs its own axis. SetWindowed() APPLIES the
        // flag rather than forcing decoration back on.

        virtual void SetDecorated(bool bInDecorated) = 0;
        virtual bool IsDecorated() const             = 0;

        // Decoration
        /*-------------------------------------------------------------------------*/

        /*-------------------------------------------------------------------------*/
        // Placement — what a client-drawn title bar needs in order to move, size and button the
        // window itself.

        /**
         * Top-left in SCREEN coordinates.
         *
         * Int32, not Uint32: a monitor left of the primary and a maximized window's -8,-8 on
         * Windows both give a legitimately negative position.
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
         * @return The backend graphics context the window owns.
         * A command-buffer backend (Vulkan) borrows the shared device/swapchain from it; the OpenGL render API ignores it.
         */
        virtual IGraphicsContext* GetGraphicsContext() const = 0;
        
        // Get - Set
        // =============================================================================
    };
}
