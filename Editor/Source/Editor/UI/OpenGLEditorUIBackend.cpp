#include "Editor/UI/OpenGLEditorUIBackend.h"

#include "RHI/Framebuffer.h"   // IFramebuffer::GetColorAttachmentID
#include "RHI/Texture.h"       // ITexture2D::GetRendererID

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
        // true = install ImGui's GLFW callbacks, chained onto the window's existing ones (the editor's
        // input relies on this).
        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 410 core");
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
        // Save/restore the current GL context around the multi-viewport render (it makes other
        // windows' contexts current).
        GLFWwindow* lCurrentContext = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(lCurrentContext);
    }

    EditorImage OpenGLEditorUIBackend::GetViewportImage(IFramebuffer& InFB)
    {
        // The GL color attachment name is the ImGui texture handle. The framebuffer is stored bottom-up,
        // so V is flipped to show the world upright.
        return { static_cast<Uint64>(InFB.GetColorAttachmentID()),
                 Vector2F(0.f, 1.f), Vector2F(1.f, 0.f) };
    }

    EditorImage OpenGLEditorUIBackend::GetTextureImage(ITexture2D& InTexture)
    {
        // Same as above: the GL texture name is the handle, and the pixels are bottom-up.
        return { static_cast<Uint64>(InTexture.GetRendererID()),
                 Vector2F(0.f, 1.f), Vector2F(1.f, 0.f) };
    }
}
