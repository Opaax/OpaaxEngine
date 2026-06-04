#pragma once
#if OPAAX_WITH_EDITOR && OPAAX_HAS_VULKAN

#include "Editor/UI/IEditorUIBackend.h"

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Opaax
{
    class IGraphicsContext;
    class VulkanContext;
    class VulkanDevice;
    class VulkanSwapchain;

    // =============================================================================
    // VulkanEditorUIBackend
    // =============================================================================
    /**
     * @class VulkanEditorUIBackend
     *
     * IEditorUIBackend over ImGui_ImplVulkan + ImGui_ImplGlfw (InitForVulkan) — the only place
     * imgui_impl_vulkan is named (mirrors imgui_impl_opengl3 living only in OpenGLEditorUIBackend).
     *
     * Borrows the live VulkanDevice + VulkanSwapchain from the graphics context. ImGui draws are
     * recorded into the per-frame command buffer inside a swapchain dynamic-rendering pass (VK 1.3
     * dynamic rendering, matching the renderer). Single-window for now — multi-viewport (secondary
     * swapchains) lands in M8 Phase 4e.
     */
    class VulkanEditorUIBackend final : public IEditorUIBackend
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        VulkanEditorUIBackend(GLFWwindow* InWindow, IGraphicsContext* InContext);
        ~VulkanEditorUIBackend() override;

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

        EditorViewportImage GetViewportImage(IFramebuffer& InFB) override;
        //~End IEditorUIBackend interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        GLFWwindow*      m_Window    = nullptr;
        VulkanContext*   m_Context   = nullptr;
        VulkanDevice*    m_Device    = nullptr;   // borrowed from the context
        VulkanSwapchain* m_Swapchain = nullptr;   // borrowed from the context

        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        VkFormat         m_ColorFormat    = VK_FORMAT_UNDEFINED;   // must outlive ImGui_ImplVulkan_Init
        bool             m_Initialized    = false;

        // Cached ImGui descriptor set for the ViewportPanel's offscreen image. Rebuilt when the
        // framebuffer's generation changes (resize recreated its image/view).
        VkDescriptorSet  m_ViewportTexture    = VK_NULL_HANDLE;
        Uint64           m_ViewportTextureGen = 0;
    };

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR && OPAAX_HAS_VULKAN
