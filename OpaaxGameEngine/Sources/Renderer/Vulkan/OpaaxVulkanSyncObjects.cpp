#include "Renderer/Vulkan/OpaaxVulkanSyncObjects.h"

#include <stdexcept>

#include "Renderer/Vulkan/OpaaxVulkanGlobal.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanSyncObjects::OpaaxVulkanSyncObjects(VkDevice LogicalDevice)
{
    CreateSyncObjects(LogicalDevice);
}

OpaaxVulkanSyncObjects::~OpaaxVulkanSyncObjects() {}

void OpaaxVulkanSyncObjects::CreateSyncObjects(VkDevice LogicalDevice)
{
    m_vkImageAvailableSemaphores.resize(VULKAN_CONST::MAX_FRAMES_IN_FLIGHT);
    m_vkRenderFinishedSemaphores.resize(VULKAN_CONST::MAX_FRAMES_IN_FLIGHT);
    m_vkInFlightFences.resize(VULKAN_CONST::MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo lSemaphoreInfo{};
    lSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < VULKAN_CONST::MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(LogicalDevice, &lSemaphoreInfo, nullptr, &m_vkImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(LogicalDevice, &lSemaphoreInfo, nullptr, &m_vkRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(LogicalDevice, &fenceInfo, nullptr, &m_vkInFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void OpaaxVulkanSyncObjects::Cleanup(VkDevice LogicalDevice)
{
    for (size_t i = 0; i < VULKAN_CONST::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(LogicalDevice, m_vkRenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(LogicalDevice, m_vkImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(LogicalDevice, m_vkInFlightFences[i], nullptr);
    }
}
