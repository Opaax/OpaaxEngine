#pragma once

#include "Window/Window.h"
#include "Application/Services/ILogger.h"
#include "GLFW/glfw3.h"

namespace Opaax
{
    class IGraphicsContext;
    
    inline constexpr LogCategory LogWindowsWindow{"WindowsWindow"};

    /**
     * @class WindowsWindow
     */
    class WindowsWindow : public Window
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        WindowsWindow(const WindowProps& Props);
        ~WindowsWindow() override;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        void Init(const WindowProps& Props);
        void RegisterGLFWCallbacks();
        
        // =============================================================================
        // Overrides
        // =============================================================================
        
        //~Begin Window interface
    public:
        Uint32 GetWidth() const override { return m_Data.Width; }
        Uint32 GetHeight() const override { return m_Data.Height; }

		virtual void PollEvents() override;
        virtual bool ShouldClose() const override;
        virtual void RequestClose() override;
        virtual void SwapBuffers() override;
        virtual void Shutdown() override;

        void* GetNativeWindow() const override { return m_Window; }

        IGraphicsContext* GetGraphicsContext() const override { return m_Context.get(); }

        void SetEventCallback(const EventCallbackFunc& Callback) override { m_Data.EventCallback = Callback; }
        
        /*-------------------------------------------------------------------------*/
        // Window Mode
        
        void        SetWindowMode(EWindowMode NewWindowMode) override;
        EWindowMode GetWindowMode() const override { return m_Data.Mode; }
        
        void SetWindowed()      override;
        void SetBorderless()    override;
        void SetFullscreen()    override;
        
        void SaveWindowedState() override;

        // Window Mode
        /*-------------------------------------------------------------------------*/

        /*-------------------------------------------------------------------------*/
        // Decoration

        void SetDecorated(bool bInDecorated) override;
        bool IsDecorated() const override;

        // Decoration
        /*-------------------------------------------------------------------------*/

        /*-------------------------------------------------------------------------*/
        // Placement

        void GetPosition(Int32& OutX, Int32& OutY) const override;
        void SetPosition(Int32 InX, Int32 InY) override;
        void SetSize(Uint32 InWidth, Uint32 InHeight) override;

        void Minimize() override;
        void Maximize() override;
        void Restore() override;
        bool IsMaximized() const override;

        // Placement
        /*-------------------------------------------------------------------------*/
        //~End Window interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window;

        // Backend graphics context — owns make-current, glad load, vsync, present.
        TUniquePtr<IGraphicsContext> m_Context;

        struct WindowData
        {
            OpaaxString Title;

            // SIGNED: a monitor left of the primary gives a negative X, and SaveWindowedState
            // writes glfwGetWindowPos straight in here.
            Int32 PosX = 0, PosY = 0;

            Uint32 Width, Height;
            Uint32 RefreshRate = GLFW_DONT_CARE;
            EWindowMode Mode;

            // The host's STATED preference, which SetWindowed re-applies. Borderless overrides it
            // while active without overwriting it, so leaving borderless restores what was asked for.
            bool bDecorated = true;

            EventCallbackFunc EventCallback;
        };

        WindowData  m_Data;
    };
}

