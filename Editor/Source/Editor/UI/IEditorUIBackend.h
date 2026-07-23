#pragma once

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorUIBackend — the renderer side of the editor's ImGui integration.
    //
    //   ImGui_ImplGlfw is the platform side (backend-neutral apart from its InitForX
    //   variant); the renderer impl (ImGui_ImplOpenGL3 today, ImGui_ImplVulkan later) is
    //   backend-specific. EditorService keeps all backend-neutral ImGui calls (context,
    //   NewFrame, dockspace, Render) and delegates these renderer hooks here, so editor
    //   code never names a concrete renderer impl. Lives editor-side so the engine/RHI
    //   stay ImGui-free (Editor.md D4).
    //
    //   M0 shape: the five frame-lifecycle hooks only. The viewport-to-FBO handles
    //   (GetViewportImage / GetTextureID) and the multi-backend Create factory land at M1.
    //
    //   No OPAAX_API: OpaaxEditorLib is a static lib archived into the editor exe, not a DLL.
    // =============================================================================
    class IEditorUIBackend
    {
        // =============================================================================
        // Dtor
        // =============================================================================
    public:
        virtual ~IEditorUIBackend() = default;
        
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         *  ImGui_ImplGlfw_InitForX + the renderer impl Init.
         *  The ImGui context must exist first.
         */
        virtual void Init() = 0;

        /** Renderer impl Shutdown + ImGui_ImplGlfw_Shutdown. 
         * Before ImGui::DestroyContext. 
         */
        virtual void Shutdown() = 0;

        /**
         * Renderer impl NewFrame + ImGui_ImplGlfw_NewFrame.
         * Before ImGui::NewFrame.
         */
        virtual void NewFrame() = 0;

        /**
         * Renderer impl RenderDrawData(ImGui::GetDrawData()).
         * After ImGui::Render.
         */
        virtual void RenderDrawData() = 0;
        
        /**
         * Multi-viewport update + default render, wrapped in any backend-specific current-context save/restore.
         * The caller gates on ImGuiConfigFlags_ViewportsEnable.
         */
        virtual void RenderPlatformWindows() = 0;
    };
}
