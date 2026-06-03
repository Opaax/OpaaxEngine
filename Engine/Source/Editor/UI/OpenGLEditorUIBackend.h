#pragma once
#if OPAAX_WITH_EDITOR

#include "Editor/UI/IEditorUIBackend.h"

struct GLFWwindow;

namespace Opaax
{
    // =============================================================================
    // OpenGLEditorUIBackend
    // =============================================================================
    /**
     * @class OpenGLEditorUIBackend
     *
     * IEditorUIBackend over ImGui_ImplOpenGL3 + ImGui_ImplGlfw (InitForOpenGL).
     * The only place imgui_impl_opengl3 is named — a future VulkanEditorUIBackend
     * sits behind the same interface.
     */
    class OPAAX_API OpenGLEditorUIBackend final : public IEditorUIBackend
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLEditorUIBackend(GLFWwindow* InWindow);

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IEditorUIBackend interface
    public:
        void Init()                  override;
        void Shutdown()              override;
        void NewFrame()              override;
        void RenderDrawData()        override;
        void RenderPlatformWindows() override;

        Uint64 GetViewportTextureID(IFramebuffer& InFB) override;
        //~End IEditorUIBackend interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow* m_Window = nullptr;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
