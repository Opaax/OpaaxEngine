#pragma once

#include "Editor/UI/IEditorUIBackend.h"

struct GLFWwindow;

namespace Opaax::Editor
{
    // =============================================================================
    // OpenGLEditorUIBackend — IEditorUIBackend over ImGui_ImplOpenGL3 + ImGui_ImplGlfw
    //   (InitForOpenGL). The only place imgui_impl_opengl3 is named; a future
    //   VulkanEditorUIBackend sits behind the same interface (M1/VK). The new render path
    //   is OpenGL-only today (S7), so M0 constructs this directly.
    // =============================================================================
    class OpenGLEditorUIBackend final : public IEditorUIBackend
    {
    public:
        explicit OpenGLEditorUIBackend(GLFWwindow* InWindow);

        //~Begin IEditorUIBackend interface
        void Init()                  override;
        void Shutdown()              override;
        void NewFrame()              override;
        void RenderDrawData()        override;
        void RenderPlatformWindows() override;

        EditorImage GetViewportImage(IFramebuffer& InFB)     override;
        EditorImage GetTextureImage(ITexture2D& InTexture)   override;
        //~End IEditorUIBackend interface

    private:
        GLFWwindow* m_Window = nullptr;
    };
}
