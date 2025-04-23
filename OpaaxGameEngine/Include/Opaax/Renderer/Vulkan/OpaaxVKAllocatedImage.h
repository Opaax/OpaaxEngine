#pragma once
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OpaaxVKAllocatedImage
            {
                VkImage         Image       = VK_NULL_HANDLE;
                VkImageView     ImageView   = VK_NULL_HANDLE;
                VmaAllocation   Allocation  = nullptr;
                VkExtent3D      ImageExtent {};
                VkFormat        ImageFormat = VK_FORMAT_UNDEFINED;
            };
        }
    }
}
