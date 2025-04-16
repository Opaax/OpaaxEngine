#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace OPAAX
{
    namespace VULKAN
    {
        using VecVkSemaphore    = std::vector<VkSemaphore>;
        using VecVkFence        = std::vector<VkFence>;

        using VecVkCommandBuffers = std::vector<VkCommandBuffer>;

        using VecVkImgView = std::vector<VkImageView>;

        using VecVkFrameBuffer = std::vector<VkFramebuffer>;
    }
}
