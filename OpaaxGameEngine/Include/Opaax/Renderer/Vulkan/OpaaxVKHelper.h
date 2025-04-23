#pragma once
#include <set>
#include <SDL3/SDL_vulkan.h>

#include "OpaaxVKGlobal.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/Log/OPLogMacro.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN_HELPER
        {
            static std::vector<const char*> GetRequiredExtensions()
            {
                UInt32 lExtensionCount = 0;
                const char* const* lFetchedExtensions = SDL_Vulkan_GetInstanceExtensions(&lExtensionCount);

                if (lFetchedExtensions == NULL)
                {
                    OPAAX_ERROR("[OpaaxVKInstance]: Failed to extension for VK instance!")
                    throw std::runtime_error("Failed to extension for VK instance!");
                }

                std::vector<const char*> lExtensions(lFetchedExtensions, lFetchedExtensions + lExtensionCount);

                if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
                {
                    lExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                }

                return lExtensions;
            }

            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                                VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                                const VkDebugUtilsMessengerCallbackDataEXT*
                                                                pCallbackData,
                                                                void* pUserData)
            {
                OPAAX_DEBUG("Validation Layer %1%", %pCallbackData->pMessage)
                std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;

                return VK_FALSE;
            }

            FORCEINLINE VkCommandPoolCreateInfo CommandPoolCreateInfo(UInt32 QueueFamilyIndex,
                                                                      VkCommandPoolCreateFlags Flags = 0)
            {
                /*
                 * create a command pool for commands submitted to the graphics queue.
                 * we also want the pool to allow for resetting of individual command buffers
                 * By doing that ` = {}` thing, we are letting the compiler initialize the entire struct to zero.
                 * This is critical, as in general Vulkan structs will have their defaults set in a way that 0 is relatively safe.
                 */
                VkCommandPoolCreateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
                lInfo.pNext = nullptr;
                lInfo.queueFamilyIndex = QueueFamilyIndex;
                lInfo.flags = Flags;
                return lInfo;
            }

            FORCEINLINE VkCommandBufferAllocateInfo CommandBufferAllocateInfo(VkCommandPool Pool, UInt32 Count = 1)
            {
                VkCommandBufferAllocateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                lInfo.pNext = nullptr;

                lInfo.commandPool = Pool;
                lInfo.commandBufferCount = Count;
                lInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                return lInfo;
            }


            /**
             * Creates and returns a new FenceCreateInfo object, which is used to specify parameters for creating a synchronization fence.
             * @https://docs.vulkan.org/spec/latest/chapters/synchronization.html#VkFenceCreateInfo
             * @param Flags A bitmask specifying the initial state and configuration of the fence. Flags can control fence behaviors like signaled state.
             * @return A FenceCreateInfo instance containing the specified flags and name setup for the synchronization fence creation.
             */
            FORCEINLINE VkFenceCreateInfo FenceCreateInfo(VkFenceCreateFlags Flags = 0)
            {
                VkFenceCreateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                lInfo.pNext = nullptr;
                lInfo.flags = Flags;

                return lInfo;
            }

            /**
             * Represents the configuration required to create a semaphore, a synchronization primitive used to coordinate operations between different GPU queues or between the CPU and GPU.            * @https://docs.vulkan.org/spec/latest/chapters/synchronization.html#VkSemaphoreCreateInfo
             * @param Flags A bitmask defining additional options for the semaphore creation. This can typically include reserved or implementation-defined flags.
             * @return A SemaphoreCreateInfo object initialized with the specified parameters, suitable for semaphore creation.
             */
            FORCEINLINE VkSemaphoreCreateInfo SemaphoreCreateInfo(VkSemaphoreCreateFlags Flags = 0)
            {
                VkSemaphoreCreateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                lInfo.pNext = nullptr;
                lInfo.flags = Flags;

                return lInfo;
            }

            /**
             * Creates and returns a VkSemaphoreSubmitInfo structure, which is used to specify parameters for semaphore submission in Vulkan.
             * @https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSemaphoreSubmitInfo
             * @param StageMask A bitmask of VkPipelineStageFlags2 specifying the stage(s) of the pipeline the semaphore waits for or signals at.
             * @param Semaphore The Vulkan semaphore handle to be associated with this submission info structure.
             * @return A VkSemaphoreSubmitInfo structure initialized with the specified stage mask, semaphore, and default parameters.
             */
            FORCEINLINE VkSemaphoreSubmitInfo SemaphoreSubmitInfo(VkPipelineStageFlags2 StageMask, VkSemaphore Semaphore)
            {
                VkSemaphoreSubmitInfo lSubmitInfo{};
                lSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                lSubmitInfo.pNext = nullptr;
                lSubmitInfo.semaphore = Semaphore;
                lSubmitInfo.stageMask = StageMask;
                lSubmitInfo.deviceIndex = 0;
                lSubmitInfo.value = 1;

                return lSubmitInfo;
            }

            FORCEINLINE VkCommandBufferBeginInfo CommandBufferBeginInfo(VkCommandBufferUsageFlags Flags = 0)
            {
                VkCommandBufferBeginInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                lInfo.pNext = nullptr;
                lInfo.pInheritanceInfo = nullptr;
                lInfo.flags = Flags;

                return lInfo;
            }

            /**
             * Creates and returns a new VkCommandBufferSubmitInfo object, used to specify a command buffer for submission.
             * @https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkCommandBufferSubmitInfo
             * @param CommandBuffer The command buffer to be associated with the VkCommandBufferSubmitInfo structure.
             * @return A VkCommandBufferSubmitInfo instance initialized with the provided command buffer and default parameters.
             */
            inline VkCommandBufferSubmitInfo CommandBufferSubmitInfo(VkCommandBuffer CommandBuffer)
            {
                VkCommandBufferSubmitInfo lInfo{};
                lInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                lInfo.pNext = nullptr;
                lInfo.commandBuffer = CommandBuffer;
                lInfo.deviceMask = 0;

                return lInfo;
            }

            /**
             * Prepares and returns a VkSubmitInfo2 structure, used for queue submission in Vulkan.
             * This function allows specifying command buffers, signal semaphore, and wait semaphore information for a submission.
             *
             * @https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VkSubmitInfo2
             *
             * @param CommandBuffer A pointer to a VkCommandBufferSubmitInfo structure, which defines the command buffer to be submitted to the queue.
             * @param SignalSemaphoreInfo A pointer to a VkSemaphoreSubmitInfo structure, which describes the semaphore to signal upon queue completion. Pass nullptr if no signal semaphore is required.
             * @param WaitSemaphoreInfo A pointer to a VkSemaphoreSubmitInfo structure, which details the semaphore that the queue must wait on before executing the submitted command buffer. Pass nullptr if no wait semaphore is required.
             * @return A VkSubmitInfo2 structure populated with the provided command buffer and semaphore information.
             */
            static VkSubmitInfo2 SubmitInfo(const VkCommandBufferSubmitInfo* CommandBuffer, const VkSemaphoreSubmitInfo* SignalSemaphoreInfo, const VkSemaphoreSubmitInfo* WaitSemaphoreInfo)
            {
                VkSubmitInfo2 lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                lInfo.pNext = nullptr;

                lInfo.waitSemaphoreInfoCount = WaitSemaphoreInfo == nullptr ? 0 : 1;
                lInfo.pWaitSemaphoreInfos = WaitSemaphoreInfo;

                lInfo.signalSemaphoreInfoCount = SignalSemaphoreInfo == nullptr ? 0 : 1;
                lInfo.pSignalSemaphoreInfos = SignalSemaphoreInfo;

                lInfo.commandBufferInfoCount = 1;
                lInfo.pCommandBufferInfos = CommandBuffer;

                return lInfo;
            }

            FORCEINLINE VkImageSubresourceRange ImageSubResourceRange(VkImageAspectFlags AspectMask)
            {
                VkImageSubresourceRange lSubImage{};
                lSubImage.aspectMask = AspectMask;
                lSubImage.baseMipLevel = 0;
                lSubImage.levelCount = VK_REMAINING_MIP_LEVELS;
                lSubImage.baseArrayLayer = 0;
                lSubImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

                return lSubImage;
            }

            /**
             * @https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
             * Handles the transition of an image between different layouts in a Vulkan application.
             * This is often required to prepare an image for specific operations like rendering or transfer.
             * @param CommandBuffer The command buffer used to record the image transition operations.
             * @param Image The Vulkan image object to be transitioned.
             * @param CurrentLayout The current layout of the image before the transition.
             * @param NewLayout The layout the image should be transitioned to.
             */
            static void TransitionImage(VkCommandBuffer CommandBuffer, VkImage Image, VkImageLayout CurrentLayout, VkImageLayout NewLayout)
            {
                // Initialize the VkImageMemoryBarrier2 structure
                VkImageMemoryBarrier2 lImageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                lImageBarrier.pNext = nullptr;

                // Specify the pipeline stages and access masks for the barrier
                lImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                lImageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
                lImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                lImageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

                // Define the old and new layouts for the image transition
                lImageBarrier.oldLayout = CurrentLayout;
                lImageBarrier.newLayout = NewLayout;

                // Determine the aspect mask for the image based on the new layout
                VkImageAspectFlags lAspectMask = (NewLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
                                                     ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                     : VK_IMAGE_ASPECT_COLOR_BIT;
                lImageBarrier.subresourceRange = ImageSubResourceRange(lAspectMask);
                // Set the subresource range based on aspect mask
                lImageBarrier.image = Image; // Specify the image to be transitioned

                // Initialize the VkDependencyInfo structure
                VkDependencyInfo lDepInfo{};
                lDepInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                lDepInfo.pNext = nullptr;

                // Attach the image memory barrier to the dependency info
                lDepInfo.imageMemoryBarrierCount = 1;
                lDepInfo.pImageMemoryBarriers = &lImageBarrier;

                // Issue the pipeline barrier command
                vkCmdPipelineBarrier2(CommandBuffer, &lDepInfo);
            }

            static void CopyImageToImage(VkCommandBuffer CommandBuffer, VkImage ImgSource, VkImage ImgDestination,
                                         VkExtent2D SourceSize, VkExtent2D DestinationSize) {}

            static void GenerateMipmaps(VkCommandBuffer CommandBuffer, VkImage Image, VkExtent2D ImageSize) {}
        }
    }
}
