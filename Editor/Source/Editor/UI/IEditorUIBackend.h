#pragma once

#include "Core/OpaaxTypes.h"        // Uint64
#include "Core/Maths/MathTypes.h"   // Vector2F

namespace Opaax
{
    class IFramebuffer;   // engine-side RHI types — the editor only holds references
    class ITexture2D;
}

namespace Opaax::Editor
{
    // =============================================================================
    // EditorImage — anything the editor can hand to ImGui::Image: the texture handle plus the
    //   sampling UVs that present it upright for the backend's storage convention. Handle is
    //   Uint64 so this header stays ImGui-free — the caller casts it to ImTextureID at the
    //   ImGui::Image call site.
    //
    //   The UVs travel WITH the handle rather than being a rule each call site remembers. That
    //   is the whole reason this is a struct and not a Uint64: the orientation is
    //   backend-specific (GL stores an FBO bottom-up, and TextureResource decodes bottom-up
    //   because GL samples that way), and Legacy proved what happens otherwise — four separate
    //   copies of the literal `(0,1)-(1,0)`, each with its own comment explaining it.
    // =============================================================================
    struct EditorImage
    {
        Uint64   Handle = 0;
        Vector2F UV0    = { 0.f, 0.f };
        Vector2F UV1    = { 1.f, 1.f };

        /** False when there is nothing to draw — no framebuffer, or pixels not uploaded yet. */
        bool IsValid() const noexcept { return Handle != 0; }
    };

    // =============================================================================
    // IEditorUIBackend — the renderer side of the editor's ImGui integration.
    //
    //   ImGui_ImplGlfw is the platform side (backend-neutral apart from its InitForX
    //   variant); the renderer impl (ImGui_ImplOpenGL3 today, ImGui_ImplVulkan later) is
    //   backend-specific. ImGuiEditorGui keeps all backend-neutral ImGui calls (context,
    //   NewFrame, dockspace, Render) and delegates these renderer hooks here, so editor
    //   code never names a concrete renderer impl. Lives editor-side so the engine/RHI
    //   stay ImGui-free (Editor.md D4).
    //
    //   Two display seams, and the split is by WHAT is being shown: GetViewportImage turns the
    //   world's offscreen FBO into an image (the ViewportPanel), GetTextureImage turns a loaded
    //   texture into one (icons, thumbnails, the Preview panel). There is no multi-backend Create
    //   factory — the render path is OpenGL-only (S7), so EditorService constructs
    //   OpenGLEditorUIBackend directly.
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
         * An offscreen framebuffer's color attachment, ready for ImGui::Image — the ViewportPanel
         * samples its own FBO through this. OpenGL returns the raw GL texture name with V flipped
         * (GL FBOs are stored bottom-up).
         */
        virtual EditorImage GetViewportImage(IFramebuffer& InFB) = 0;

        /**
         * A loaded texture, ready for ImGui::Image — a type icon, a browser thumbnail, the Preview
         * panel. Same V flip, and for the same reason one level down: TextureResource decodes
         * bottom-up precisely because GL samples that way.
         *
         * @return An INVALID image when the texture has no backend handle yet (decoded, not
         *   uploaded) — a normal state, not an error, so the caller draws its fallback.
         */
        virtual EditorImage GetTextureImage(ITexture2D& InTexture) = 0;
    };
}
