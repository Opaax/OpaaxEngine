#pragma once

#include "Core/Window.h"
#include "GLFW/glfw3.h"

namespace Opaax
{
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
        //~End Window interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window;
        
        struct WindowData
        {
            String Title;
            Uint32 Width, Height;
        };

        WindowData m_Data;
    };
}

