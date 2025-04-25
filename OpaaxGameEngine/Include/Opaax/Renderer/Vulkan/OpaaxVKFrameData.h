#pragma once

#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxDeletionQueue.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            /**
             * @struct OpaaxVKFrameData
             * @brief Represents the Vulkan frame data necessary for managing rendering operations within a single frame.
             *
             * This structure contains semaphores, fences, and command objects required for managing GPU synchronization and
             * rendering commands in Vulkan. It is designed to handle resources for a specific frame within the Vulkan rendering pipeline.
             */
            struct OPAAX_API OpaaxVKFrameData
            {
                //used to control presenting the image to the OS once the drawing finishes
                VkSemaphore             SwapchainSemaphore  = VK_NULL_HANDLE;
                //wait for the draw commands of a given frame to be finished
                VkSemaphore             RenderSemaphore     = VK_NULL_HANDLE;
                VkFence                 RenderFence         = VK_NULL_HANDLE;
                VkCommandPool           CommandPool         = VK_NULL_HANDLE;
                VkCommandBuffer         MainCommandBuffer   = VK_NULL_HANDLE;

                OpaaxDeletionQueue      DeletionQueue;
            };
        }
    }
}

