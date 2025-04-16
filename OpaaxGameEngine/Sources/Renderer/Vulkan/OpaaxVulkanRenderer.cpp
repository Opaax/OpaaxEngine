#include "Renderer/Vulkan/OpaaxVulkanRenderer.h"

#include "Math/OpaaxMathMacro.h"
#include "Renderer/OpaaxWindow.h"
#include "Renderer/Vulkan/OpaaxVulkanCommandBuffers.h"
#include "Renderer/Vulkan/OpaaxVulkanGlobal.h"
#include "Renderer/Vulkan/OpaaxVulkanInstance.h"
#include "Renderer/Vulkan/OpaaxVulkanSyncObjects.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanRenderer::OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window) : IOpaaxRenderer(Window)
{
    
}

OpaaxVulkanRenderer::~OpaaxVulkanRenderer()
{
    if(IsInitialized())
    {
        OPAAX_WARNING("%1% Destroying but not shutdown correctly. this may result to undef behavior.", %typeid(*this).name())
    }
}

void OpaaxVulkanRenderer::CreateSurface(const VkInstance Instance, GLFWwindow* const Window)
{
    OPAAX_LOG("[OpaaxVulkanRenderer] Creating Vulkan Window surface...")
    
    if (glfwCreateWindowSurface(Instance, Window, nullptr, &m_vkSurface) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer] Failed to create vulkan window surface!")
        throw std::runtime_error("failed to create vulkan window surface!");
    }
    
    OPAAX_LOG("[OpaaxVulkanRenderer] Vulkan Window surface created!")
}

void OpaaxVulkanRenderer::CreateRenderPass()
{
    OPAAX_LOG("[OpaaxVulkanRenderer] creating Vulkan render pass...")
    VkAttachmentDescription lColorAttachment{};
    lColorAttachment.format = m_opaaxVkSwapChain->GetSwapChainImageFormat();
    lColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    lColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    lColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    lColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    lColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    lColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    lColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference lColorAttachmentRef{};
    lColorAttachmentRef.attachment = 0;
    lColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription lSubpass{};
    lSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lSubpass.colorAttachmentCount = 1;
    lSubpass.pColorAttachments = &lColorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &lColorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &lSubpass;

    if (vkCreateRenderPass(m_opaaxVkLogicalDevice->GetDevice(), &renderPassInfo, nullptr, &m_vkRenderPass) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer] Failed to create render pass!")
        throw std::runtime_error("failed to create render pass!");
    }

    OPAAX_LOG("[OpaaxVulkanRenderer] Render Vulkan Renderpass created!")
}

void OpaaxVulkanRenderer::RecordCommandBuffer(VkCommandBuffer CommandBuffer, UInt32 ImageIndex)
{
    VkExtent2D lSCExtent = m_opaaxVkSwapChain->GetSwapChainExtent();
    
    VkCommandBufferBeginInfo lBeginInfo{};
    lBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(CommandBuffer, &lBeginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo lRenderPassInfo{};
    lRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    lRenderPassInfo.renderPass = m_vkRenderPass;
    lRenderPassInfo.framebuffer = m_opaaxFrameBuffers->GetSwapChainFrameBuffers()[ImageIndex];
    lRenderPassInfo.renderArea.offset = {0, 0};
    lRenderPassInfo.renderArea.extent = lSCExtent;

    VkClearValue lClearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    lRenderPassInfo.clearValueCount = 1;
    lRenderPassInfo.pClearValues = &lClearColor;

    vkCmdBeginRenderPass(CommandBuffer, &lRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_opaaxGraphicPipeline->GetGraphicsPipeline());

    VkViewport lViewport{};
    lViewport.x = 0.0f;
    lViewport.y = 0.0f;
    lViewport.width = static_cast<float>(lSCExtent.width);
    lViewport.height = static_cast<float>(lSCExtent.height);
    lViewport.minDepth = 0.0f;
    lViewport.maxDepth = 1.0f;
    vkCmdSetViewport(CommandBuffer, 0, 1, &lViewport);

    VkRect2D lScissor{};
    lScissor.offset = {0, 0};
    lScissor.extent = lSCExtent;
    vkCmdSetScissor(CommandBuffer, 0, 1, &lScissor);

    vkCmdDraw(CommandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(CommandBuffer);

    if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to record command buffer!");
    }
}

void OpaaxVulkanRenderer::RecreateSwapChain()
{
    Int32 lWidth = 0, lHeight = 0;
    glfwGetFramebufferSize(GetOpaaxWindow()->GetGLFWindow(), &lWidth, &lHeight);
    
    while (lWidth == 0 || lHeight == 0)
    {
        glfwGetFramebufferSize(GetOpaaxWindow()->GetGLFWindow(), &lWidth, &lHeight);
        glfwWaitEvents();
    }
    
    VkDevice lDevice = m_opaaxVkLogicalDevice->GetDevice();

    vkDeviceWaitIdle(lDevice);

    m_opaaxFrameBuffers->CleanupForRecreate(lDevice);
    m_opaaxVkSwapChain->CleanupForRecreate(lDevice);

    m_opaaxVkSwapChain->Recreate(m_opaaxVkPhysicalDevice->GetPhysicalDevice(), m_vkSurface, m_opaaxVkLogicalDevice->GetDevice());
    m_opaaxFrameBuffers->Recreate(m_opaaxVkSwapChain->GetSwapChainImageViews(), m_opaaxVkSwapChain->GetSwapChainExtent(), m_vkRenderPass, m_opaaxVkLogicalDevice->GetDevice());
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("============== [OpaaxVulkanRenderer]: Init Vulkan Renderer... ==============")
    
    m_opaaxVkInstance       = MakeUnique<OpaaxVulkanInstance>();
    CreateSurface(m_opaaxVkInstance->GetInstance(), GetOpaaxWindow()->GetGLFWindow());
    m_opaaxVkPhysicalDevice = MakeUnique<OpaaxVulkanPhysicalDevice>(m_opaaxVkInstance->GetInstance(), m_vkSurface);
    m_opaaxVkLogicalDevice  = MakeUnique<OpaaxVulkanLogicalDevice>(m_opaaxVkPhysicalDevice->GetPhysicalDevice(), m_vkSurface);
    m_opaaxVkSwapChain      = MakeUnique<OpaaxVulkanSwapChain>(GetOpaaxWindow(), m_opaaxVkPhysicalDevice->GetPhysicalDevice(), m_vkSurface, m_opaaxVkLogicalDevice->GetDevice());
    CreateRenderPass();
    m_opaaxGraphicPipeline  = MakeUnique<OpaaxVulkanGraphicsPipeline>(m_opaaxVkLogicalDevice->GetDevice(), m_vkRenderPass);
    m_opaaxFrameBuffers     = MakeUnique<OpaaxVulkanFrameBuffers>( m_opaaxVkSwapChain->GetSwapChainImageViews(), m_opaaxVkSwapChain->GetSwapChainExtent(), m_vkRenderPass, m_opaaxVkLogicalDevice->GetDevice());
    m_opaaxCommandPool      = MakeUnique<OpaaxVulkanCommandPool>(m_opaaxVkPhysicalDevice->GetPhysicalDevice(), m_vkSurface, m_opaaxVkLogicalDevice->GetDevice());
    m_opaaxCommandBuffers   = MakeUnique<OpaaxVulkanCommandBuffers>(m_opaaxCommandPool->GetCommandPool(), m_opaaxVkLogicalDevice->GetDevice()); 
    m_opaaxSyncObjects      = MakeUnique<OpaaxVulkanSyncObjects>(m_opaaxVkLogicalDevice->GetDevice());
    
    SetIsInitialized(true);

    OPAAX_VERBOSE("============== [OpaaxVulkanRenderer]: End Init Vulkan Renderer! ==============")
    
    return IsInitialized();
}

void OpaaxVulkanRenderer::Resize()
{
    bFramebufferResized = true;
}

void OpaaxVulkanRenderer::RenderFrame()
{
    VkDevice                    lDevice                     = m_opaaxVkLogicalDevice->GetDevice();
    const VecVkCommandBuffers&  lCommandBuffers             = m_opaaxCommandBuffers->GetCommandBuffers();
    const VecVkSemaphore&       lImgAvailableSemaphores     = m_opaaxSyncObjects->GetImageAvailableSemaphores();
    const VecVkSemaphore&       lRenderFinishedSemaphores   = m_opaaxSyncObjects->GetRenderFinishedSemaphores();
    const VecVkFence&           lInFlightFences             = m_opaaxSyncObjects->GetInFlightFences();
    VkSwapchainKHR              lSwapChain                  = m_opaaxVkSwapChain->GetSwapChain();
    VkQueue                     lGraphicQueue               = m_opaaxVkLogicalDevice->GetGraphicsQueue();
    VkQueue                     lPresentQueue               = m_opaaxVkLogicalDevice->GetPresentQueue();
    
    vkWaitForFences(lDevice, 1, &lInFlightFences[m_currentFrame], VK_TRUE, OP_UMAX_64);
    vkResetFences(lDevice, 1, &lInFlightFences[m_currentFrame]);

    UInt32 lImageIndex;
    VkResult lResult = vkAcquireNextImageKHR(lDevice, lSwapChain, OP_UMAX_64, lImgAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &lImageIndex);

    if (lResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    if (lResult != VK_SUCCESS && lResult != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }
    
    vkResetCommandBuffer(lCommandBuffers[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    RecordCommandBuffer(lCommandBuffers[m_currentFrame], lImageIndex);

    VkSubmitInfo lSubmitInfo{};
    lSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore lWaitSemaphores[] = {lImgAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    lSubmitInfo.waitSemaphoreCount = 1;
    lSubmitInfo.pWaitSemaphores = lWaitSemaphores;
    lSubmitInfo.pWaitDstStageMask = waitStages;

    lSubmitInfo.commandBufferCount = 1;
    lSubmitInfo.pCommandBuffers = &lCommandBuffers[m_currentFrame];

    VkSemaphore lSignalSemaphores[] = {lRenderFinishedSemaphores[m_currentFrame]};
    lSubmitInfo.signalSemaphoreCount = 1;
    lSubmitInfo.pSignalSemaphores = lSignalSemaphores;

    if (vkQueueSubmit(lGraphicQueue, 1, &lSubmitInfo, lInFlightFences[m_currentFrame]) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR lPresentInfo{};
    lPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    lPresentInfo.waitSemaphoreCount = 1;
    lPresentInfo.pWaitSemaphores = lSignalSemaphores;

    VkSwapchainKHR lSwapChains[] = {lSwapChain};
    lPresentInfo.swapchainCount = 1;
    lPresentInfo.pSwapchains = lSwapChains;

    lPresentInfo.pImageIndices = &lImageIndex;

    lResult = vkQueuePresentKHR(lPresentQueue, &lPresentInfo);

    if (lResult == VK_ERROR_OUT_OF_DATE_KHR || lResult == VK_SUBOPTIMAL_KHR || bFramebufferResized)
    {
        bFramebufferResized = false;
        RecreateSwapChain();
    }
    else if (lResult != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % VULKAN_CONST::MAX_FRAMES_IN_FLIGHT;
}
void OpaaxVulkanRenderer::Shutdown()
{
    OPAAX_VERBOSE("============== [OpaaxVulkanRenderer]: Shutting down Vulkan Renderer... ==============")

    VkDevice lDevice = m_opaaxVkLogicalDevice->GetDevice();

    vkDeviceWaitIdle(lDevice);
    
    m_opaaxSyncObjects->Cleanup(lDevice);
    m_opaaxCommandPool->Cleanup(lDevice);
    m_opaaxFrameBuffers->Cleanup(lDevice);
    m_opaaxGraphicPipeline->Cleanup(lDevice);
    vkDestroyRenderPass(lDevice, m_vkRenderPass, nullptr);
    m_opaaxVkSwapChain->Cleanup(lDevice);
    m_opaaxVkLogicalDevice->Cleanup();
    vkDestroySurfaceKHR(m_opaaxVkInstance->GetInstance(), m_vkSurface, nullptr);
    m_opaaxVkInstance->Cleanup();

    SetIsInitialized(false);
    
    OPAAX_VERBOSE("============== [OpaaxVulkanRenderer]: End Shutting down Vulkan Renderer! ==============")
}
