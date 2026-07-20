#include "Editor/UI/OpenGLEditorUIBackend.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace Opaax::Editor
{
    OpenGLEditorUIBackend::OpenGLEditorUIBackend(GLFWwindow* InWindow)
        : m_Window(InWindow)
    {
    }

    void OpenGLEditorUIBackend::Init()
    {
        // true = install ImGui's GLFW callbacks, CHAINED onto the window's existing ones (set by the
        // engine before the window handed off). M-Input's editor track relies on this chaining (S10 10b).
        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 450");
    }

    void OpenGLEditorUIBackend::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }

    void OpenGLEditorUIBackend::NewFrame()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void OpenGLEditorUIBackend::RenderDrawData()
    {
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void OpenGLEditorUIBackend::RenderPlatformWindows()
    {
        // Save/restore the current GL context around the multi-viewport render —
        // RenderPlatformWindowsDefault makes other windows' contexts current.
        GLFWwindow* lCurrentContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(lCurrentContext);
    }
}
