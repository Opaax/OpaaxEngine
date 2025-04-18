#pragma once

#include <GLFW/glfw3.h>
#include "Opaax/Window/OpaaxWindow.h"

namespace OPAAX
{
    inline bool GbIsGLFWInitialized = false;
    
    class OPAAX_API OpaaxWindowsWindow : public OpaaxWindow
    {
    private:
        GLFWwindow* m_window;

        struct OPWindowData
        {
            OSTDString Title{};
            UInt32 Width{}, Height{};
            bool bVSync{};

            OPWindowData() = default;

            OPWindowData(const OSTDString& InTitle, UInt32 InWidth, UInt32 InHeight) : Title(InTitle), Width(InWidth),
                Height(InHeight) {}
        };

        OPWindowData m_windowData;

    public:
        OpaaxWindowsWindow(const OpaaxWindowSpecs& Specs);
        ~OpaaxWindowsWindow() override;
        
        virtual void Init() override;
        virtual void Shutdown() override;

    public:
        void OnUpdate() override;
        UInt32 GetWidth() const override { return m_windowData.Width; }
        UInt32 GetHeight() const override { return m_windowData.Height; }
        // Window attributes
        //void SetEventCallback(const EventCallbackFn& callback) override { m_Data.EventCallback = callback; }
        void SetVSync(bool Enabled) override;
        bool IsVSync() const override { return m_windowData.bVSync; }
        void* GetNativeWindow() const override { return m_window; }
        bool ShouldClose() override;
    };
}
