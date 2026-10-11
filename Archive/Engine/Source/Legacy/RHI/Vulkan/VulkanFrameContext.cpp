#include "VulkanFrameContext.h"

#if OPAAX_HAS_VULKAN

#include "VulkanDevice.h"

namespace Opaax
{
    VulkanDevice* VulkanFrameContext::s_Device      = nullptr;
    VkFormat      VulkanFrameContext::s_ColorFormat = VK_FORMAT_UNDEFINED;
    Uint32        VulkanFrameContext::s_FrameSlot   = 0;
    Uint64        VulkanFrameContext::s_Generation  = 0;

    VmaAllocator VulkanFrameContext::Allocator() noexcept
    {
        return s_Device ? s_Device->GetAllocator() : nullptr;
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
