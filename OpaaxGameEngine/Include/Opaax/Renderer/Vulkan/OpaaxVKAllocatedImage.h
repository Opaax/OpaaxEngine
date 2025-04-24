#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxVKAllocatedImage
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
