#pragma once
#include <vulkan/vulkan_core.h>

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            using VecVkSemaphore            = std::vector<VkSemaphore>;
            using VecVkFence                = std::vector<VkFence>;

            using VecVkCommandBuffers       = std::vector<VkCommandBuffer>;

            using VecVkImgView              = std::vector<VkImageView>;
            using VecVkImg                  = std::vector<VkImage>;

            using VecVkFrameBuffer          = std::vector<VkFramebuffer>;

            using VecDescSetLayoutBinding   = std::vector<VkDescriptorSetLayoutBinding>;
        }
    }
}
