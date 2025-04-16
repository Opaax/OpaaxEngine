#pragma once
#include "OpaaxTypes.h"
#include "GLFW/glfw3.h"


namespace OPAAX
{
    class IOpaaxRenderer;
    
    class OpaaxWindow
    {
        GLFWwindow*     m_window{nullptr};
        IOpaaxRenderer* m_renderer{nullptr};
        
        Int32   m_width{0};
        Int32   m_height{0};
        OString m_windowName{};
        bool    bFramebufferResized{false};

    public:
        OpaaxWindow() = default;
        ~OpaaxWindow() = default; //Do not manage for deleting Rendere and/or window

        OpaaxWindow(const OpaaxWindow&) = delete;        // Non-copyable
        OpaaxWindow& operator=(const OpaaxWindow&) = delete;

        OpaaxWindow(OpaaxWindow&&) = default;            // Movable
        OpaaxWindow& operator=(OpaaxWindow&&) = default;

    private:
        static void FramebufferResizeCallback(GLFWwindow* Window, Int32 Width, Int32 Height);

        void NotifyResizeRenderer();

    public:
        void InitWindow(Int32 Width, Int32 Height, const OString& WindowName);
        void LinkRenderer(IOpaaxRenderer* Renderer) { m_renderer = Renderer; }
        void PollEvents();
        void Cleanup() const;

        FORCEINLINE bool        ShouldClose()   const { return glfwWindowShouldClose(m_window); }
        FORCEINLINE GLFWwindow* GetGLFWindow()  const { return m_window; }
        FORCEINLINE OString     GetWindowName() const { return m_windowName; }
        FORCEINLINE Int32       GetWidth()      const { return m_width; }
        FORCEINLINE Int32       GetHeight()     const { return m_height; }
    };
}
