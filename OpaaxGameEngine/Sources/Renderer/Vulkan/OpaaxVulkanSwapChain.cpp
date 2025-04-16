#include "Renderer/Vulkan/OpaaxVulkanSwapChain.h"

#include <GLFW/glfw3.h>

#include "Renderer/OpaaxWindow.h"
#include "Renderer/Vulkan/OpaaxVulkanHelper.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanSwapChain::OpaaxVulkanSwapChain(OpaaxWindow* Window, VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface,
    VkDevice LogicalDevice): m_window(Window)
{
    OPAAX_VERBOSE("============== [OpaaxVulkanSwapChain]: Init Vulkan Swap Chain... ==============")
    
    CreateSwapChain(PhysicalDevice, Surface, LogicalDevice);
    CreateImageViews(LogicalDevice);
    
    bIsInitialized = true;
    
    OPAAX_VERBOSE("============== [OpaaxVulkanSwapChain]: End Init Vulkan SwapChain. ==============")
}

OpaaxVulkanSwapChain::~OpaaxVulkanSwapChain()
{
    if(bIsInitialized)
    {
        OPAAX_WARNING("%1% Destroying but not clean. this may result to undef behavior.", %typeid(*this).name())
    }
}

VkSurfaceFormatKHR OpaaxVulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& AvailableFormats)
{
    for (const auto& lAvailableFormat : AvailableFormats)
    {
        if (lAvailableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && lAvailableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return lAvailableFormat;
        }
    }

    return AvailableFormats[0];
}

VkPresentModeKHR OpaaxVulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& AvailablePresentModes)
{
    for (const auto& lAvailablePresentMode : AvailablePresentModes)
    {
        if (lAvailablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return lAvailablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D OpaaxVulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    if (Capabilities.currentExtent.width != std::numeric_limits<UInt32>::max()) {
        return Capabilities.currentExtent;
    }

    Int32 lWidth, lHeight;
    glfwGetFramebufferSize(m_window->GetGLFWindow(), &lWidth, &lHeight);

    VkExtent2D lActualExtent =
    {
        static_cast<UInt32>(lWidth),
        static_cast<UInt32>(lHeight)
    };

    lActualExtent.width = std::clamp(lActualExtent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
    lActualExtent.height = std::clamp(lActualExtent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);

    return lActualExtent;
}

void OpaaxVulkanSwapChain::CreateSwapChain(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice)
{
    OPAAX_LOG("[OpaaxVulkanSwapChain] Creating Vulkan swap chain...")
    
    OpaaxSwapChainSupportDetails lSwapChainSupport = VULKAN_HELPER::QuerySwapChainSupport(PhysicalDevice, Surface);

    VkSurfaceFormatKHR  lSurfaceFormat  = ChooseSwapSurfaceFormat(lSwapChainSupport.Formats);
    VkPresentModeKHR    lPresentMode    = ChooseSwapPresentMode(lSwapChainSupport.PresentModes);
    VkExtent2D          lExtent         = ChooseSwapExtent(lSwapChainSupport.Capabilities);

    UInt32 lImageCount = lSwapChainSupport.Capabilities.minImageCount + 1;
    if (lSwapChainSupport.Capabilities.maxImageCount > 0 && lImageCount > lSwapChainSupport.Capabilities.maxImageCount)
    {
        lImageCount = lSwapChainSupport.Capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    lCreateInfo.surface = Surface;

    lCreateInfo.minImageCount = lImageCount;
    lCreateInfo.imageFormat = lSurfaceFormat.format;
    lCreateInfo.imageColorSpace = lSurfaceFormat.colorSpace;
    lCreateInfo.imageExtent = lExtent;
    lCreateInfo.imageArrayLayers = 1;
    lCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    OpaaxQueueFamilyIndices lIndices = VULKAN_HELPER::FindQueueFamilies(PhysicalDevice, Surface);
    UInt32 lQueueFamilyIndices[] = {lIndices.GraphicsFamily.value(), lIndices.PresentFamily.value()};

    if (lIndices.GraphicsFamily != lIndices.PresentFamily)
    {
        lCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        lCreateInfo.queueFamilyIndexCount = 2;
        lCreateInfo.pQueueFamilyIndices = lQueueFamilyIndices;
    }
    else
    {
        lCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    lCreateInfo.preTransform = lSwapChainSupport.Capabilities.currentTransform;
    lCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    lCreateInfo.presentMode = lPresentMode;
    lCreateInfo.clipped = VK_TRUE;

    lCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(LogicalDevice, &lCreateInfo, nullptr, &m_vkSwapChain) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanSwapChain] Failed to create swap chain!")
        throw std::runtime_error("Failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(LogicalDevice, m_vkSwapChain, &lImageCount, nullptr);
    m_vkSwapChainImages.resize(lImageCount);
    vkGetSwapchainImagesKHR(LogicalDevice, m_vkSwapChain, &lImageCount, m_vkSwapChainImages.data());

    m_vkSwapChainImageFormat = lSurfaceFormat.format;
    m_vkSwapChainExtent = lExtent;

    OPAAX_LOG("[OpaaxVulkanSwapChain] Vulkan swap chain created!")
}

void OpaaxVulkanSwapChain::CreateImageViews(VkDevice LogicalDevice)
{
    m_vkSwapChainImageViews.resize(m_vkSwapChainImages.size());

    for (size_t i = 0; i < m_vkSwapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_vkSwapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_vkSwapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(LogicalDevice, &createInfo, nullptr, &m_vkSwapChainImageViews[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanSwapChain] Failed to create image views!")
            throw std::runtime_error("Failed to create image views!");
        }
    }
}

void OpaaxVulkanSwapChain::Cleanup(VkDevice LogicalDevice)
{
    OPAAX_VERBOSE("============== [OpaaxVulkanSwapChain]: Start Cleanup ==============")

    for (auto lImageView : m_vkSwapChainImageViews)
    {
        vkDestroyImageView(LogicalDevice, lImageView, nullptr);
    }
    
    vkDestroySwapchainKHR(LogicalDevice, m_vkSwapChain, nullptr);
    
    bIsInitialized = false;

    OPAAX_VERBOSE("============== [OpaaxVulkanSwapChain]: End Cleanup ==============")
}

void OpaaxVulkanSwapChain::CleanupForRecreate(VkDevice LogicalDevice)
{
    for (auto lImageView : m_vkSwapChainImageViews)
    {
        vkDestroyImageView(LogicalDevice, lImageView, nullptr);
    }
    
    vkDestroySwapchainKHR(LogicalDevice, m_vkSwapChain, nullptr);
}

void OpaaxVulkanSwapChain::Recreate(VkPhysicalDevice PhysicalDevice, VkSurfaceKHR Surface, VkDevice LogicalDevice)
{
    CreateSwapChain(PhysicalDevice, Surface, LogicalDevice);
    CreateImageViews(LogicalDevice);
}
