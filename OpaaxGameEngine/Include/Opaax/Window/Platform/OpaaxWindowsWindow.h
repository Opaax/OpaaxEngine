#pragma once
#include "Opaax/Renderer/IOpaaxRendererContext.h"
#include "Opaax/Window/OpaaxWindow.h"

struct SDL_Window;

namespace OPAAX
{
    inline bool GbIsNativeWindowInitialized = false;
    
    class OPAAX_API OpaaxWindowsWindow : public OpaaxWindow
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        SDL_Window*                         m_window            = nullptr;

        struct OPWindowData
        {
            OSTDString Title{};
            UInt32 Width{}, Height{};
            bool bVSync{};

            OPWindowData() = default;

            OPWindowData(const OSTDString& InTitle, UInt32 InWidth, UInt32 InHeight) : Title(InTitle), Width(InWidth),
                Height(InHeight) {}
        } m_windowData;

        //-----------------------------------------------------------------
        // CTOR / DTOR
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        OpaaxWindowsWindow(const OpaaxWindowSpecs& Specs);
        ~OpaaxWindowsWindow() override;

        //-----------------------------------------------------------------
        // Function
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
        void InitSDLWindow();

        //-----------------------------------------------------------------
        // Override
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        virtual void Initialize()         override;
        virtual void PollEvents()   override;
        virtual void OnUpdate()     override;
        virtual void Shutdown()     override;

        UInt32  GetWidth()          const   override { return m_windowData.Width; }
        UInt32  GetHeight()         const   override { return m_windowData.Height; }
        bool    IsVSync()           const   override { return m_windowData.bVSync; }
        void*   GetNativeWindow()   const   override { return m_window; }
        void    SetVSync(bool Enabled)      override;
    };
}
