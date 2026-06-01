#pragma once

#include "Core/Window.h"
#include "GLFW/glfw3.h"

namespace Opaax
{
    class IGraphicsContext;

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
        void Shutdown();
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
        virtual void SwapBuffers() override;

        void* GetNativeWindow() const override { return m_Window; }

        IGraphicsContext* GetGraphicsContext() const override { return m_Context.get(); }

        void SetEventCallback(const EventCallbackFunc& Callback) override { m_Data.EventCallback = Callback; }
        //~End Window interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window;

        // Backend graphics context — owns make-current, glad load, vsync, present.
        UniquePtr<IGraphicsContext> m_Context;

        struct WindowData
        {
            String Title;
            Uint32 Width, Height;

            EventCallbackFunc EventCallback;
        };

        WindowData m_Data;
    };
}

