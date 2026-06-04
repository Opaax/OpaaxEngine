#include "VulkanDevice.h"

#if OPAAX_HAS_VULKAN

// NOTE: VMA_IMPLEMENTATION lives in its own TU (VulkanVMA.cpp) — defining it here would be
//   swallowed by vk_mem_alloc.h's include guard (VulkanDevice.h already pulled the header).

#include "Core/Log/OpaaxLog.h"

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>

namespace Opaax
{
    namespace
    {
        const char* DeviceTypeName(VkPhysicalDeviceType InType)
        {
            switch (InType)
            {
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated";
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:   return "Discrete";
                case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:    return "Virtual";
                case VK_PHYSICAL_DEVICE_TYPE_CPU:            return "CPU";
                default:                                     return "Other";
            }
        }

        const char* VendorName(Uint32 InVendorID)
        {
            switch (InVendorID)
            {
                case 0x10DE: return "NVIDIA";
                case 0x1002: return "AMD";
                case 0x8086: return "Intel";
                case 0x13B5: return "ARM";
                case 0x5143: return "Qualcomm";
                default:     return "Unknown";
            }
        }
    }

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

        VkPhysicalDeviceProperties lProps{};
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &lProps);

        OPAAX_CORE_INFO("====================  Render Backend  ====================");
        OPAAX_CORE_INFO("  API .............. Vulkan {}.{}.{}",
                        VK_API_VERSION_MAJOR(lProps.apiVersion),
                        VK_API_VERSION_MINOR(lProps.apiVersion),
                        VK_API_VERSION_PATCH(lProps.apiVersion));
        OPAAX_CORE_INFO("  GPU .............. {} ({})", lProps.deviceName, DeviceTypeName(lProps.deviceType));
        OPAAX_CORE_INFO("  Vendor ........... {} (0x{:04X})", VendorName(lProps.vendorID), lProps.vendorID);
        OPAAX_CORE_INFO("  Driver ........... 0x{:08X}", lProps.driverVersion);
        OPAAX_CORE_INFO("  Queues ........... graphics family {} ({})", m_GraphicsQueueFamily,
                        (m_PresentQueue == m_GraphicsQueue) ? "present shared" : "present separate");
        OPAAX_CORE_INFO("  VMA .............. {}", m_Allocator ? "ready" : "FAILED");
        OPAAX_CORE_INFO("==========================================================");
    }

    void VulkanDevice::ImmediateSubmit(const TFunction<void(VkCommandBuffer)>& InRecord) const
    {
        // Transient pool + buffer for a single synchronous submission. Cheap relative to the
        // texture decode it wraps; textures only load at startup / asset import, not per frame.
        VkCommandPoolCreateInfo lPoolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        lPoolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        lPoolInfo.queueFamilyIndex = m_GraphicsQueueFamily;

        VkCommandPool lPool = VK_NULL_HANDLE;
        if (vkCreateCommandPool(m_Device, &lPoolInfo, nullptr, &lPool) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanDevice::ImmediateSubmit — command pool creation failed.");
            return;
        }

        VkCommandBufferAllocateInfo lAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        lAllocInfo.commandPool        = lPool;
        lAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        lAllocInfo.commandBufferCount = 1;

        VkCommandBuffer lCmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(m_Device, &lAllocInfo, &lCmd);

        VkCommandBufferBeginInfo lBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        lBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(lCmd, &lBegin);

        InRecord(lCmd);

        vkEndCommandBuffer(lCmd);

        VkSubmitInfo lSubmit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        lSubmit.commandBufferCount = 1;
        lSubmit.pCommandBuffers    = &lCmd;
        vkQueueSubmit(m_GraphicsQueue, 1, &lSubmit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);

        vkDestroyCommandPool(m_Device, lPool, nullptr);
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
