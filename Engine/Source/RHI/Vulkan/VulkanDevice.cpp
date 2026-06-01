#include "VulkanDevice.h"

#if OPAAX_HAS_VULKAN

// NOTE: VMA_IMPLEMENTATION lives in its own TU (VulkanVMA.cpp) — defining it here would be
//   swallowed by vk_mem_alloc.h's include guard (VulkanDevice.h already pulled the header).

#include "Core/Log/OpaaxLog.h"

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    VulkanDevice::VulkanDevice(GLFWwindow* InWindow)
    {
        // ---- Instance --------------------------------------------------------
        vkb::InstanceBuilder lInstanceBuilder;
        lInstanceBuilder.set_app_name("Opaax")
                        .require_api_version(1, 3, 0);
#ifndef NDEBUG
        lInstanceBuilder.request_validation_layers(true)
                        .use_default_debug_messenger();
#endif
        auto lInstRet = lInstanceBuilder.build();
        if (!lInstRet)
        {
            OPAAX_CORE_ERROR("VulkanDevice: instance build failed: {}", lInstRet.error().message());
            return;
        }
        vkb::Instance lVkbInstance = lInstRet.value();
        m_Instance       = lVkbInstance.instance;
        m_DebugMessenger = lVkbInstance.debug_messenger;

        // ---- Surface ---------------------------------------------------------
        const VkResult lSurfRes = glfwCreateWindowSurface(m_Instance, InWindow, nullptr, &m_Surface);
        if (lSurfRes != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanDevice: glfwCreateWindowSurface failed ({}).", static_cast<int>(lSurfRes));
            return;
        }

        // ---- Physical device (require dynamic rendering + sync2, VK 1.3) -----
        VkPhysicalDeviceVulkan13Features lFeatures13{};
        lFeatures13.sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        lFeatures13.dynamicRendering = VK_TRUE;
        lFeatures13.synchronization2 = VK_TRUE;

        vkb::PhysicalDeviceSelector lSelector{ lVkbInstance };
        auto lPhysRet = lSelector.set_surface(m_Surface)
                                 .set_minimum_version(1, 3)
                                 .set_required_features_13(lFeatures13)
                                 .require_present(true)
                                 .select();
        if (!lPhysRet)
        {
            OPAAX_CORE_ERROR("VulkanDevice: no suitable GPU (VK 1.3 + dynamic rendering): {}",
                             lPhysRet.error().message());
            return;
        }
        vkb::PhysicalDevice lVkbPhys = lPhysRet.value();
        m_PhysicalDevice = lVkbPhys.physical_device;

        // ---- Logical device + queues ----------------------------------------
        vkb::DeviceBuilder lDeviceBuilder{ lVkbPhys };
        auto lDevRet = lDeviceBuilder.build();
        if (!lDevRet)
        {
            OPAAX_CORE_ERROR("VulkanDevice: device build failed: {}", lDevRet.error().message());
            return;
        }
        vkb::Device lVkbDevice = lDevRet.value();
        m_Device = lVkbDevice.device;

        auto lGfxQueue   = lVkbDevice.get_queue(vkb::QueueType::graphics);
        auto lGfxFamily  = lVkbDevice.get_queue_index(vkb::QueueType::graphics);
        auto lPresQueue  = lVkbDevice.get_queue(vkb::QueueType::present);
        if (!lGfxQueue || !lGfxFamily || !lPresQueue)
        {
            OPAAX_CORE_ERROR("VulkanDevice: failed to retrieve graphics/present queues.");
            return;
        }
        m_GraphicsQueue       = lGfxQueue.value();
        m_GraphicsQueueFamily = lGfxFamily.value();
        m_PresentQueue        = lPresQueue.value();

        // ---- VMA allocator ---------------------------------------------------
        VmaAllocatorCreateInfo lAllocInfo{};
        lAllocInfo.physicalDevice   = m_PhysicalDevice;
        lAllocInfo.device           = m_Device;
        lAllocInfo.instance         = m_Instance;
        lAllocInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        if (vmaCreateAllocator(&lAllocInfo, &m_Allocator) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanDevice: VMA allocator creation failed.");
            m_Allocator = nullptr;
        }

        OPAAX_CORE_INFO("VulkanDevice: ready (graphics queue family {}).", m_GraphicsQueueFamily);
    }

    VulkanDevice::~VulkanDevice()
    {
        // Reverse order of creation.
        if (m_Allocator)      { vmaDestroyAllocator(m_Allocator); }
        if (m_Device)         { vkDestroyDevice(m_Device, nullptr); }
        if (m_Surface)        { vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr); }
#ifndef NDEBUG
        if (m_DebugMessenger) { vkb::destroy_debug_utils_messenger(m_Instance, m_DebugMessenger); }
#endif
        if (m_Instance)       { vkDestroyInstance(m_Instance, nullptr); }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
