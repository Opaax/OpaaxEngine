#pragma once

#include "Editor/UI/IEditorUIBackend.h"

struct GLFWwindow;

namespace Opaax::Editor
{
    // =============================================================================
    // OpenGLEditorUIBackend — IEditorUIBackend over ImGui_ImplOpenGL3 + ImGui_ImplGlfw. The only
    //   place imgui_impl_opengl3 is named; a Vulkan version would implement the same interface.
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
