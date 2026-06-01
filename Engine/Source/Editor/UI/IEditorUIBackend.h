#pragma once
#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/RenderAPI.h"

namespace Opaax
{
    // =============================================================================
    // IEditorUIBackend
    // =============================================================================
    /**
     * @interface IEditorUIBackend
     *
     * The renderer side of the editor's ImGui integration. ImGui_ImplGlfw is the
     * platform side (backend-neutral apart from its InitForX variant) while the
     * renderer impl (ImGui_ImplOpenGL3 today, ImGui_ImplVulkan later) is backend-
     * specific. EditorSubsystem keeps all backend-neutral ImGui calls and delegates
     * these four renderer hooks here, so the editor never names a concrete renderer
     * impl. The concrete is selected by IEditorUIBackend::Create, defined in the
     * active backend's TU (OpenGLEditorUIBackend.cpp) — mirrors RenderAPI::Create.
     *
     * Lives editor-side (not in RHI/) so RHI stays ImGui-free.
     */
    class OPAAX_API IEditorUIBackend
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IEditorUIBackend() = default;

        // =============================================================================
        // Statics
        // =============================================================================

        /**
         * @param InBackend    selected graphics backend
         * @param InGlfwWindow native GLFW window handle (for ImGui_ImplGlfw_InitForX)
         * @return owning backend, or nullptr on unknown backend (logged)
         */
        static UniquePtr<IEditorUIBackend> Create(EBackend InBackend, void* InGlfwWindow);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // ImGui_ImplGlfw_InitForX + the renderer impl Init. ImGui context must exist first.
        virtual void Init() = 0;

        // Renderer impl Shutdown + ImGui_ImplGlfw_Shutdown. Before ImGui::DestroyContext.
        virtual void Shutdown() = 0;

        // Renderer impl NewFrame + ImGui_ImplGlfw_NewFrame. Before ImGui::NewFrame.
        virtual void NewFrame() = 0;

        // Renderer impl RenderDrawData(ImGui::GetDrawData()). After ImGui::Render.
        virtual void RenderDrawData() = 0;

        // Multi-viewport update + default render, wrapped in any backend-specific
        // current-context save/restore. Caller gates on ImGuiConfigFlags_ViewportsEnable.
        virtual void RenderPlatformWindows() = 0;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
