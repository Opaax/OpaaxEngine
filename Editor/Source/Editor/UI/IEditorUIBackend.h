#pragma once

#include "Core/OpaaxTypes.h"        // Uint64
#include "Core/Maths/MathTypes.h"   // Vector2F

namespace Opaax
{
    class IFramebuffer;   // engine-side RHI type — the editor only holds a reference
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorViewportImage — a displayable offscreen color attachment: the ImGui texture
    //   handle plus the sampling UVs that present it upright for the backend's storage
    //   convention. Handle is Uint64 so this header stays ImGui-free — the caller casts it
    //   to ImTextureID at the ImGui::Image call site.
    // =============================================================================
    struct EditorViewportImage
    {
        Uint64   Handle = 0;
        Vector2F UV0    = { 0.f, 0.f };
        Vector2F UV1    = { 1.f, 1.f };
    };

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
    //   M1 shape: the five frame-lifecycle hooks + GetViewportImage (the ViewportPanel samples
    //   its offscreen FBO through it). GetTextureID (asset thumbnails) is M2. There is no
    //   multi-backend Create factory — the render path is OpenGL-only (S7), so EditorService
    //   constructs OpenGLEditorUIBackend directly.
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

        /**
         * The displayable handle + upright sampling UVs for an offscreen framebuffer's color
         * attachment — the ViewportPanel feeds both to ImGui::Image. OpenGL returns the raw GL
         * texture name with V flipped (GL FBOs are stored bottom-up). Handle is Uint64 so this
         * header stays ImGui-free. (GetTextureID for asset thumbnails lands at M2.)
         */
        virtual EditorViewportImage GetViewportImage(IFramebuffer& InFB) = 0;
    };
}
