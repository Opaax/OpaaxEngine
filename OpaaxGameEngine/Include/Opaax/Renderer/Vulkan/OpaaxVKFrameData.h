#pragma once
#include "OpaaxVKDeletionQueue.h"
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OpaaxVKFrameData
            {
                //used to control presenting the image to the OS once the drawing finishes
                VkSemaphore             SwapchainSemaphore  = VK_NULL_HANDLE;
                //wait for the draw commands of a given frame to be finished
                VkSemaphore             RenderSemaphore     = VK_NULL_HANDLE;
                VkFence                 RenderFence         = VK_NULL_HANDLE;
                VkCommandPool           CommandPool         = VK_NULL_HANDLE;
                VkCommandBuffer         MainCommandBuffer   = VK_NULL_HANDLE;

                OpaaxVKDeletionQueue    DeletionQueue;
            };
        }
    }
}

