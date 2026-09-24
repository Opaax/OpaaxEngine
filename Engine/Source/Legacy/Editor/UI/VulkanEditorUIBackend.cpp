#include "VulkanEditorUIBackend.h"
#if OPAAX_WITH_EDITOR && OPAAX_HAS_VULKAN

#include "RHI/Vulkan/VulkanContext.h"
#include "RHI/Vulkan/VulkanDevice.h"
#include "RHI/Vulkan/VulkanSwapchain.h"
#include "RHI/Vulkan/VulkanCommandBuffer.h"
#include "RHI/Vulkan/VulkanFramebuffer.h"
#include "RHI/Vulkan/VulkanTexture2D.h"
#include "RHI/RenderCommand.h"
#include "RHI/ICommandBuffer.h"
#include "Renderer/RenderTarget.hpp"
#include "Core/Log/OpaaxLog.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    VulkanEditorUIBackend::VulkanEditorUIBackend(GLFWwindow* InWindow, IGraphicsContext* InContext)
        : m_Window(InWindow)
    {
        // The render API already initialized the Vulkan context (RenderSubsystem runs before the
        // editor) — borrow its device + swapchain. Downcast is a backend invariant here.
        m_Context = static_cast<VulkanContext*>(InContext);
        if (m_Context)
        {
            m_Device      = &m_Context->GetDevice();
            m_Swapchain   = &m_Context->GetSwapchain();
            m_ColorFormat = m_Swapchain->GetImageFormat();
        }
    }

    VulkanEditorUIBackend::~VulkanEditorUIBackend() = default;

    void VulkanEditorUIBackend::Init()
    {
        if (!m_Device || !m_Swapchain)
        {
            OPAAX_CORE_ERROR("VulkanEditorUIBackend: no Vulkan context — cannot init the ImGui Vulkan backend.");
            return;
        }

        // Descriptor pool for ImGui (font atlas + the AddTexture viewport image + one set per
        // asset-browser thumbnail, plus headroom). FREE_DESCRIPTOR_SET lets RemoveTexture recycle
        // a set on viewport resize.
        VkDescriptorPoolSize lPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256 };

        VkDescriptorPoolCreateInfo lPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
        lPoolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        lPoolInfo.maxSets       = 256;
        lPoolInfo.poolSizeCount = 1;
        lPoolInfo.pPoolSizes    = &lPoolSize;
        if (vkCreateDescriptorPool(m_Device->GetDevice(), &lPoolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanEditorUIBackend: descriptor pool creation failed.");
            return;
        }

        ImGui_ImplGlfw_InitForVulkan(m_Window, true);

        // Dynamic rendering (VK 1.3) — no VkRenderPass; the main-viewport pipeline is built against
        // the swapchain color format. m_ColorFormat must outlive this call (member, not a temp).
        ImGui_ImplVulkan_InitInfo lInfo{};
        lInfo.ApiVersion          = VK_API_VERSION_1_3;
        lInfo.Instance            = m_Device->GetInstance();
        lInfo.PhysicalDevice      = m_Device->GetPhysicalDevice();
        lInfo.Device              = m_Device->GetDevice();
        lInfo.QueueFamily         = m_Device->GetGraphicsQueueFamily();
        lInfo.Queue               = m_Device->GetGraphicsQueue();
        lInfo.DescriptorPool      = m_DescriptorPool;
        lInfo.MinImageCount       = OPAAX_FRAMES_IN_FLIGHT;
        lInfo.ImageCount          = OPAAX_FRAMES_IN_FLIGHT;
        lInfo.UseDynamicRendering = true;
        lInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        lInfo.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        lInfo.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount    = 1;
        lInfo.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &m_ColorFormat;

        ImGui_ImplVulkan_Init(&lInfo);

        m_Initialized = true;
        OPAAX_CORE_INFO("VulkanEditorUIBackend: ImGui Vulkan backend initialized (dynamic rendering).");
    }

    void VulkanEditorUIBackend::Shutdown()
    {
        if (!m_Initialized) { return; }

        // The GPU may still be drawing the last frame's ImGui — drain before tearing the backend
        // + pool down (Lesson 22).
        RenderCommand::WaitIdle();

        if (m_ViewportTexture != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(m_ViewportTexture);
            m_ViewportTexture = VK_NULL_HANDLE;
        }

        for (auto& [lView, lSet] : m_TextureCache)
        {
            ImGui_ImplVulkan_RemoveTexture(lSet);
        }
        m_TextureCache.clear();

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        if (m_DescriptorPool)
        {
            vkDestroyDescriptorPool(m_Device->GetDevice(), m_DescriptorPool, nullptr);
            m_DescriptorPool = VK_NULL_HANDLE;
        }
        m_Initialized = false;
    }

    void VulkanEditorUIBackend::NewFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
    }

    void VulkanEditorUIBackend::RenderDrawData()
    {
        ImDrawData* lDrawData = ImGui::GetDrawData();
        if (!lDrawData) { return; }

        auto&           lCmdBuf = static_cast<VulkanCommandBuffer&>(RenderCommand::GetCommandBuffer());
        VkCommandBuffer lVkCmd  = lCmdBuf.GetVkCommandBuffer();
        if (lVkCmd == VK_NULL_HANDLE) { return; }   // frame skipped (swapchain recreate)

        // Draw ImGui over the cleared backbuffer: a swapchain Load pass (no clear — keeps the
        // editor's BeginFrame clear + the world/overlay output that already presents underneath).
        // The swapchain image is already in COLOR (BeginFrame's clear pass acquired it), so this
        // BeginRenderPass won't re-transition.
        const VkExtent2D    lExtent = m_Swapchain->GetExtent();
        DefaultRenderTarget lBackbuffer(lExtent.width, lExtent.height);

        lCmdBuf.BeginRenderPass(lBackbuffer, ELoadOp::Load, Vector4F(0.f, 0.f, 0.f, 1.f));
        ImGui_ImplVulkan_RenderDrawData(lDrawData, lVkCmd);
        lCmdBuf.EndRenderPass();
    }

    EditorViewportImage VulkanEditorUIBackend::GetViewportImage(IFramebuffer& InFB)
    {
        auto&        lFB  = static_cast<VulkanFramebuffer&>(InFB);
        const Uint64 lGen = lFB.GetGeneration();

        // Rebuild the cached set when the framebuffer's image/view changed (resize bumps generation).
        if (m_ViewportTexture == VK_NULL_HANDLE || lGen != m_ViewportTextureGen)
        {
            if (m_ViewportTexture != VK_NULL_HANDLE)
            {
                // The old view/image are already gone (resize WaitIdle'd + destroyed them); freeing
                // the descriptor set is CPU-only and safe.
                ImGui_ImplVulkan_RemoveTexture(m_ViewportTexture);
                m_ViewportTexture = VK_NULL_HANDLE;
            }

            if (lFB.GetColorImageView() != VK_NULL_HANDLE && lFB.GetSampler() != VK_NULL_HANDLE)
            {
                m_ViewportTexture = ImGui_ImplVulkan_AddTexture(
                    lFB.GetSampler(), lFB.GetColorImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // Only stamp the generation on a successful mint — a null result keeps the rebuild
                // condition true so the next frame retries instead of caching a dead handle.
                if (m_ViewportTexture != VK_NULL_HANDLE) { m_ViewportTextureGen = lGen; }
            }
        }

        // A null handle here would make ImGui bind a null descriptor set (VUID-...-06563/08600). The
        // ViewportPanel guards against drawing it; log once so the cause (null view/sampler vs failed
        // AddTexture) is visible without a debugger.
        if (m_ViewportTexture == VK_NULL_HANDLE && !m_LoggedNullViewport)
        {
            m_LoggedNullViewport = true;
            const VkExtent2D lExt = lFB.GetExtent();
            OPAAX_CORE_WARN("VulkanEditorUIBackend: viewport image unresolved — view={} sampler={} "
                            "extent={}x{} (ImGui descriptor not minted).",
                            static_cast<const void*>(lFB.GetColorImageView()),
                            static_cast<const void*>(lFB.GetSampler()),
                            lExt.width, lExt.height);
        }

        // The offscreen image is top-down and rendered with a negative viewport, so sample straight.
        return { reinterpret_cast<Uint64>(m_ViewportTexture),
                 Vector2F(0.f, 0.f), Vector2F(1.f, 1.f) };
    }

    Uint64 VulkanEditorUIBackend::GetTextureID(ITexture2D& InTex)
    {
        auto&             lTex     = static_cast<VulkanTexture2D&>(InTex);
        const VkImageView lView    = lTex.GetImageView();
        const VkSampler   lSampler = lTex.GetSampler();
        if (lView == VK_NULL_HANDLE || lSampler == VK_NULL_HANDLE) { return 0; }

        // Mint the ImGui descriptor set once per texture (keyed by its stable image view) and reuse
        // it every frame — AddTexture allocates from m_DescriptorPool, so repeating it would exhaust
        // the pool.
        auto lIt = m_TextureCache.find(lView);
        if (lIt == m_TextureCache.end())
        {
            const VkDescriptorSet lSet = ImGui_ImplVulkan_AddTexture(
                lSampler, lView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (lSet == VK_NULL_HANDLE) { return 0; }   // caller (TypeActions) guards null
            lIt = m_TextureCache.emplace(lView, lSet).first;
        }

        return reinterpret_cast<Uint64>(lIt->second);
    }

    void VulkanEditorUIBackend::RenderPlatformWindows()
    {
        // Single-window on Vulkan for now — ViewportsEnable is gated off when backend == Vulkan
        // (EditorSubsystem::Startup), so the editor never calls this yet. Multi-viewport (secondary
        // swapchains) is wired in M8 Phase 4e; the standard pair is here for when it lands.
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

} // namespace Opaax

#endif // OPAAX_WITH_EDITOR && OPAAX_HAS_VULKAN
