#include "OpenGLEditorUIBackend.h"
#if OPAAX_WITH_EDITOR

#include "Core/Log/OpaaxLog.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    // =============================================================================
    // IEditorUIBackend factory
    // =============================================================================
    UniquePtr<IEditorUIBackend> IEditorUIBackend::Create(EBackend InBackend, void* InGlfwWindow)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLEditorUIBackend>(static_cast<GLFWwindow*>(InGlfwWindow));
        }

        OPAAX_CORE_ERROR("IEditorUIBackend::Create — unknown backend; no UI backend created.");
        return nullptr;
    }

    // =============================================================================
    // OpenGLEditorUIBackend
    // =============================================================================
    OpenGLEditorUIBackend::OpenGLEditorUIBackend(GLFWwindow* InWindow)
        : m_Window(InWindow)
    {
    }

    void OpenGLEditorUIBackend::Init()
    {
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

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
