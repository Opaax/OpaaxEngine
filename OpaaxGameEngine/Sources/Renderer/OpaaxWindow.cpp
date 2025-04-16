#include "Renderer/OpaaxWindow.h"
#include "Core/OPLogMacro.h"
#include "Renderer/IOpaaxRenderer.h"

using namespace OPAAX;

void OpaaxWindow::FramebufferResizeCallback(GLFWwindow* Window, Int32 Width, Int32 Height)
{
    auto lOPWin = reinterpret_cast<OpaaxWindow*>(glfwGetWindowUserPointer(Window));
    
    lOPWin->bFramebufferResized = true;
    lOPWin->m_width = Width;
    lOPWin->m_height = Height;
    lOPWin->NotifyResizeRenderer();
}

void OpaaxWindow::NotifyResizeRenderer()
{
    if(m_renderer)
    {
        m_renderer->Resize();
    }

    bFramebufferResized = false;
}

void OpaaxWindow::InitWindow(Int32 Width, Int32 Height, const OString& WindowName)
{
    OPAAX_VERBOSE("============== [OpaaxWindow] Start Init ==============")
    
    m_width = Width;
    m_height = Height;
    m_windowName = WindowName;
    
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_width, m_height, m_windowName.c_str(), nullptr, nullptr);

    OPAAX_LOG("[OpaaxWindow] Window created")
    
    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferResizeCallback);

    OPAAX_LOG("[OpaaxWindow] Size Callback bounded")
    OPAAX_VERBOSE("============== [OpaaxWindow] End Init ==============")
}

void OpaaxWindow::PollEvents()
{
    glfwPollEvents();
}

void OpaaxWindow::Cleanup() const
{
    OPAAX_VERBOSE("============== [OpaaxWindow] Start clean up =================")
    if(m_window)
    {
        glfwDestroyWindow(m_window);
        OPAAX_LOG("[OpaaxWindow] Window destroyed")
    }
    glfwTerminate();
    OPAAX_LOG("[OpaaxWindow] glfw Terminate")
    OPAAX_VERBOSE("============== [OpaaxWindow] End clean up =================")
}
