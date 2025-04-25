#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            /**
             * @struct OpaaxVKAllocatedImage
             * @brief Represents a Vulkan image along with its associated resources for rendering or computation.
             *
             * This structure encapsulates a Vulkan image, its image view, memory allocation, extent,
             * and format. It is used to manage and interact with Vulkan images in a renderer context.
             *
             * The `Image` field contains the Vulkan image handle. The `ImageView` field corresponds to
             * the associated image view, which is required for rendering or image operations.
             * The `Allocation` field holds the memory allocation information managed by the Vulkan
             * Memory Allocator (VMA). The `ImageExtent` specifies the dimensions of the image, while
             * `ImageFormat` defines the format of the image (e.g., RGBA, float32, etc.).
             *
             * These members are initialized with default values, indicating that no image or memory
             * is allocated upon its creation.
             *
             * This structure is designed for use with the Vulkan API in conjunction with the Vulkan
             * Memory Allocator library and simplifies image resource management.
             */
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
