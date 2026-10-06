#pragma once

#include "Core/OpaaxTypes.h"        // Uint64
#include "Core/Maths/MathTypes.h"   // Vector2F

namespace Opaax
{
    class IFramebuffer;   // engine RHI types, held by reference
    class ITexture2D;
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorImage — something the editor can pass to ImGui::Image: a texture handle plus the UVs that
    //   show it upright (GL stores framebuffers and textures bottom-up). The handle is a Uint64 so this
    //   header stays ImGui-free; callers cast it to ImTextureID.
    // =============================================================================
    struct EditorImage
    {
        Uint64   Handle = 0;
        Vector2F UV0    = { 0.f, 0.f };
        Vector2F UV1    = { 1.f, 1.f };

        /** False when there is nothing to draw (no framebuffer, or pixels not uploaded yet). */
        bool IsValid() const noexcept { return Handle != 0; }
    };

    // =============================================================================
    // IEditorUIBackend — the renderer side of the editor's ImGui integration. ImGuiEditorGui does the
    //   backend-neutral ImGui work and calls these renderer hooks, so editor code never names a
    //   renderer. Kept editor-side so the engine stays ImGui-free.
    //   GetViewportImage shows the world's framebuffer (ViewportPanel); GetTextureImage shows a loaded
    //   texture (icons, thumbnails, Preview). OpenGL only for now.
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

        /** Renderer impl Shutdown + ImGui_ImplGlfw_Shutdown. Before ImGui::DestroyContext. */
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
         * Multi-viewport update and render, with any backend-specific context save/restore.
         * The caller checks ImGuiConfigFlags_ViewportsEnable.
         */
        virtual void RenderPlatformWindows() = 0;

        /**
         * A framebuffer's color attachment, ready for ImGui::Image (the ViewportPanel's world image).
         * V is flipped (GL framebuffers are stored bottom-up).
         */
        virtual EditorImage GetViewportImage(IFramebuffer& InFB) = 0;

        /**
         * A loaded texture, ready for ImGui::Image (icons, thumbnails, the Preview panel). V is flipped.
         * @return An invalid image when the texture is not uploaded yet (normal; draw a fallback)
         */
        virtual EditorImage GetTextureImage(ITexture2D& InTexture) = 0;
    };
}
