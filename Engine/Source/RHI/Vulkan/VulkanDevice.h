#pragma once

#include "Core/OpaaxTypes.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

struct GLFWwindow;

namespace Opaax
{
    // =============================================================================
    // VulkanDevice
    // =============================================================================
    /**
     * @class VulkanDevice
     *
     * The Vulkan instance + surface + physical/logical device + queues + VMA allocator,
     * built once via vk-bootstrap. Owned by VulkanContext; VulkanRenderAPI borrows it
     * (non-owning) to record/submit while the context presents. Requires Vulkan 1.3 with
     * dynamic rendering + synchronization2.
     */
    class VulkanDevice
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanDevice(GLFWwindow* InWindow);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice&)            = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        // =============================================================================
        // Get
        // =============================================================================
    public:
        bool IsValid() const noexcept { return m_Device != VK_NULL_HANDLE; }

        VkInstance       GetInstance()           const noexcept { return m_Instance; }
        VkSurfaceKHR     GetSurface()            const noexcept { return m_Surface; }
        VkPhysicalDevice GetPhysicalDevice()     const noexcept { return m_PhysicalDevice; }
        VkDevice         GetDevice()             const noexcept { return m_Device; }
        VkQueue          GetGraphicsQueue()      const noexcept { return m_GraphicsQueue; }
        VkQueue          GetPresentQueue()       const noexcept { return m_PresentQueue; }
        Uint32           GetGraphicsQueueFamily() const noexcept { return m_GraphicsQueueFamily; }
        VmaAllocator     GetAllocator()          const noexcept { return m_Allocator; }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        // Record + submit a one-shot transient command buffer on the graphics queue and wait
        // idle. Used for outside-the-frame-loop GPU work (texture staging copies). Synchronous —
        // not for per-frame draw work.
        void ImmediateSubmit(const TFunction<void(VkCommandBuffer)>& InRecord) const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VkInstance               m_Instance            = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger      = VK_NULL_HANDLE;
        VkSurfaceKHR             m_Surface             = VK_NULL_HANDLE;
        VkPhysicalDevice         m_PhysicalDevice      = VK_NULL_HANDLE;
        VkDevice                 m_Device              = VK_NULL_HANDLE;
        VkQueue                  m_GraphicsQueue       = VK_NULL_HANDLE;
        VkQueue                  m_PresentQueue        = VK_NULL_HANDLE;
        Uint32                   m_GraphicsQueueFamily = 0;
        VmaAllocator             m_Allocator           = nullptr;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
