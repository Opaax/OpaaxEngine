#include "OpenGLEditorUIBackend.h"
#if OPAAX_WITH_EDITOR

#include "Core/Log/OpaaxLog.h"
#include "RHI/Framebuffer.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#if OPAAX_HAS_VULKAN
    #include "Editor/UI/VulkanEditorUIBackend.h"
#endif

namespace Opaax
{
    // =============================================================================
    // IEditorUIBackend factory
    // =============================================================================
    // The single editor-UI-backend factory (mirrors RHI/BackendFactory naming every backend in one
    // neutral spot). Lives in the always-compiled OpenGL TU; the Vulkan branch is SDK-gated.
    UniquePtr<IEditorUIBackend> IEditorUIBackend::Create(EBackend InBackend, void* InGlfwWindow, IGraphicsContext* InContext)
    {
        switch (InBackend)
        {
            case EBackend::OpenGL:
                return MakeUnique<OpenGLEditorUIBackend>(static_cast<GLFWwindow*>(InGlfwWindow));
#if OPAAX_HAS_VULKAN
            case EBackend::Vulkan:
                return MakeUnique<VulkanEditorUIBackend>(static_cast<GLFWwindow*>(InGlfwWindow), InContext);
#endif
            default: break;
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

    Uint64 OpenGLEditorUIBackend::GetViewportTextureID(IFramebuffer& InFB)
    {
        // The GL color attachment name IS the ImGui texture handle (imgui_impl_opengl3 binds it).
        return static_cast<Uint64>(InFB.GetColorAttachmentID());
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
