#pragma once
#include "Opaax/Renderer/IOpaaxRendererContext.h"
#include "Opaax/Window/OpaaxWindow.h"

struct SDL_Window;

namespace OPAAX
{
    inline bool GbIsNativeWindowInitialized = false;

    /**
     * @class OpaaxWindowsWindow
     * @brief A Windows-specific implementation of the OpaaxWindow interface.
     *
     * This class specializes in creating and managing windows using SDL for the
     * Windows platform. It provides facilities to initialize, update, and shut
     * down a window, along with handling VSync and event polling.
     */
    class OPAAX_API OpaaxWindowsWindow : public OpaaxWindow
    {
        //-----------------------------------------------------------------
        // Members
        //-----------------------------------------------------------------
        /*---------------------------- PRIVATE ----------------------------*/
    private:
        SDL_Window* m_window = nullptr;

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
        /**
         * This method handles the initialization of the SDL library in video mode. It ensures that
         * the library is properly initialized before creating or managing a window. If the SDL library
         * is already initialized, it logs a warning in debug mode. Error handling and logging are
         * key components of this method to maintain stability during the initialization process.
         */
        void InitSDLWindow();

        //-----------------------------------------------------------------
        // Override
        //-----------------------------------------------------------------
        /*---------------------------- PUBLIC ----------------------------*/
    public:
        /**
         * This method handles the initialization process for the OpaaxWindowsWindow,
         * specifically for the Windows platform. It initializes the SDL library,
         * creates an SDL window with the specified properties, and configures
         * platform-specific settings, such as VSync. Additionally, this method
         * ensures proper logging and error handling throughout the initialization.
         */
        virtual void Initialize()                   override;
        /**
         * This method retrieves the next SDL event from the queue and processes it,
         * updating the window's state and handling specific event types such as quit,
         * window minimization, maximization, keyboard input, and other input events.
         * It interacts with the input system of the OpaaxEngine for event handling.
         *
         * @param Event Reference to an SDL_Event structure that will be populated
         * with the next event from the SDL event queue.
         * @return A boolean value indicating if an event was retrieved from the queue.
         * Returns true if an event was successfully polled; otherwise, false.
         */
        virtual bool PollEvents(SDL_Event& Event)   override;
        virtual void OnUpdate()                     override;
        /**
         * This method is responsible for properly shutting down the OpaaxWindowsWindow.
         * It destroys the SDL window, resets the initialization flag, and logs the
         * shutdown process. This ensures that all resources tied to the window are
         * released before destruction, maintaining stability and preventing resource leaks.
         */
        virtual void Shutdown()                     override;

        /*---------------------------- Getter ----------------------------*/
        UInt32  GetWidth()          const   override { return m_windowData.Width; }
        UInt32  GetHeight()         const   override { return m_windowData.Height; }
        bool    IsVSync()           const   override { return m_windowData.bVSync; }
        void*   GetNativeWindow()   const   override { return m_window; }
        void    SetVSync(bool Enabled)      override;
    };
}
