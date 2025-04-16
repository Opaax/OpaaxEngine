#include "Renderer/Vulkan/OpaaxVulkanFrameBuffers.h"

#include <stdexcept>

#include "Core/OPLogMacro.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanFrameBuffers::OpaaxVulkanFrameBuffers(const std::vector<VkImageView>& SwapChainImageViews,
    VkExtent2D SwapChainExtent, VkRenderPass RenderPass, VkDevice LogicalDevice)
{
    CreateFrameBuffers(SwapChainImageViews, SwapChainExtent, RenderPass, LogicalDevice);
}

OpaaxVulkanFrameBuffers::~OpaaxVulkanFrameBuffers()
{
    
}

void OpaaxVulkanFrameBuffers::CreateFrameBuffers(
    const std::vector<VkImageView>& SwapChainImageViews, VkExtent2D SwapChainExtent,
    VkRenderPass RenderPass, VkDevice LogicalDevice)
{
    m_vkSwapChainFrameBuffers.resize(SwapChainImageViews.size());

    for (size_t i = 0; i < SwapChainImageViews.size(); i++)
    {
        VkImageView lAttachments[] = {
            SwapChainImageViews[i]
        };

        VkFramebufferCreateInfo lFramebufferInfo{};
        lFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        lFramebufferInfo.renderPass = RenderPass;
        lFramebufferInfo.attachmentCount = 1;
        lFramebufferInfo.pAttachments = lAttachments;
        lFramebufferInfo.width = SwapChainExtent.width;
        lFramebufferInfo.height = SwapChainExtent.height;
        lFramebufferInfo.layers = 1;

        if (vkCreateFramebuffer(LogicalDevice, &lFramebufferInfo, nullptr, &m_vkSwapChainFrameBuffers[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanFrameBuffers] Failed to create framebuffer!")
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void OpaaxVulkanFrameBuffers::Cleanup(VkDevice LogicalDevice)
{
    for (auto lFramebuffer : m_vkSwapChainFrameBuffers)
    {
        vkDestroyFramebuffer(LogicalDevice, lFramebuffer, nullptr);
    }
}

void OpaaxVulkanFrameBuffers::CleanupForRecreate(VkDevice LogicalDevice)
{
    for (auto lFramebuffer : m_vkSwapChainFrameBuffers)
    {
        vkDestroyFramebuffer(LogicalDevice, lFramebuffer, nullptr);
    }
}

void OpaaxVulkanFrameBuffers::Recreate(const VecVkImgView& SwapChainImageViews, VkExtent2D SwapChainExtent, VkRenderPass RenderPass, VkDevice LogicalDevice)
{
    CreateFrameBuffers(SwapChainImageViews, SwapChainExtent, RenderPass, LogicalDevice);
}
