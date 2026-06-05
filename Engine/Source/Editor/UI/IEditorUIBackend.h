#pragma once
#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxTypes.h"
#include "RHI/RenderAPI.h"

namespace Opaax
{
    class IGraphicsContext;
    class IFramebuffer;
    class ITexture2D;

    // =============================================================================
    // EditorViewportImage
    // =============================================================================
    // A displayable offscreen color attachment: the ImGui texture handle plus the
    // sampling UVs that present it upright for the backend's storage convention.
    struct EditorViewportImage
    {
        Uint64   Handle = 0;            // ImTextureID-width handle; caller casts
        Vector2F UV0    = { 0.f, 0.f };
        Vector2F UV1    = { 1.f, 1.f };
    };

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
         * @param InContext    the live graphics context (the Vulkan backend borrows its device +
         *                     swapchain; OpenGL ignores it). May be null in degenerate setups.
         * @return owning backend, or nullptr on unknown backend (logged)
         */
        static UniquePtr<IEditorUIBackend> Create(EBackend InBackend, void* InGlfwWindow, IGraphicsContext* InContext);

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

        // The displayable handle + upright sampling UVs for an offscreen framebuffer's color
        // attachment — the editor ViewportPanel feeds both to ImGui::Image. OpenGL returns the raw
        // GL texture name with V-flipped UVs (bottom-up FBO); Vulkan mints/caches a VkDescriptorSet
        // via ImGui_ImplVulkan_AddTexture with straight UVs. Handle is Uint64 so this header stays
        // ImGui-free (the caller casts to ImTextureID).
        virtual EditorViewportImage GetViewportImage(IFramebuffer& InFB) = 0;

        // The ImGui texture handle for a sampled asset texture (Asset Browser thumbnails). OpenGL
        // returns the raw GL texture name; Vulkan mints + caches a VkDescriptorSet (keyed by image
        // view) via ImGui_ImplVulkan_AddTexture. Handle is Uint64 so this header stays ImGui-free.
        // The caller owns the sampling UVs (the V-convention differs by widget, not by backend —
        // asset textures carry no render-time flip on either backend).
        virtual Uint64 GetTextureID(ITexture2D& InTex) = 0;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR
