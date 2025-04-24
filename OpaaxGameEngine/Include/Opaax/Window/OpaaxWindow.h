#pragma once
#include <SDL3/SDL_events.h>

#include "OpaaxWindowSpecs.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    /**
     * @class OpaaxWindow
     * @brief An abstract interface representing a graphical window.
     *
     * The OpaaxWindow class provides an interface for managing and interacting with a graphical window.
     * Derived classes must implement the pure virtual functions to define specific behaviors
     * for updating the window, retrieving its dimensions, toggling vertical synchronization (VSync),
     * accessing window-specific native handles, and other platform-specific features.
     */
    class OPAAX_API OpaaxWindow
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        bool bShouldQuit = false;
        bool bIsMinimized = false;

        //-----------------------------------------------------------------
        // CTOR/DTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual ~OpaaxWindow() = default;

        //-----------------------------------------------------------------
        // Functions
        //-----------------------------------------------------------------
        /*---------------------------- PROTECTED ----------------------------*/
    protected:
        virtual void SetShouldQuit(bool NewVal)     { bShouldQuit = NewVal; }
        virtual void SetIsMinimized(bool NewVal)    { bIsMinimized = NewVal; }

        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual void Initialize()                   = 0;
        virtual bool PollEvents(SDL_Event& Event)   = 0;
        virtual void OnUpdate()                     = 0;
        virtual void Shutdown()                     = 0;
        
        /*---------------------------- Getter / Setters ----------------------------*/
        virtual UInt32  GetWidth()              const   = 0;
        virtual UInt32  GetHeight()             const   = 0;
        virtual bool    IsVSync()               const   = 0;
        virtual void*   GetNativeWindow()       const   = 0;
        
        virtual void    SetVSync(bool Enabled)          = 0;
        
        FORCEINLINE virtual bool ShouldClose() { return bShouldQuit; }
        FORCEINLINE virtual bool IsMinimized() { return bIsMinimized; }
    };
}
