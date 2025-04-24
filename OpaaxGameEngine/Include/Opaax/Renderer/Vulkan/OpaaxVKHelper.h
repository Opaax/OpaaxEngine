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

            /**
             * Creates and returns a VkImageCreateInfo object, which specifies parameters for creating a Vulkan image.
             * This function configures the image properties such as format, usage flags, and extent.
             * 
             * @param Format The format of the image, specifying the type and layout of texel blocks.
             * @param UsageFlags A bitmask specifying intended usage of the image, such as transfer or sampling operations.
             * @param Extent The dimensions of the image, defined as a 3D extent with width, height, and depth.
             * @return A VkImageCreateInfo instance configured with the provided parameters for image creation.
             */
            FORCEINLINE VkImageCreateInfo ImageCreateInfo(VkFormat Format, VkImageUsageFlags UsageFlags, VkExtent3D Extent)
            {
                VkImageCreateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
                lInfo.pNext = nullptr;

                lInfo.imageType = VK_IMAGE_TYPE_2D;

                lInfo.format = Format;
                lInfo.extent = Extent;

                lInfo.mipLevels = 1;
                lInfo.arrayLayers = 1;

                //for MSAA. we will not be using it by default, so default it to 1 sample per pixel.
                lInfo.samples = VK_SAMPLE_COUNT_1_BIT;

                //optimal tiling, which means the image is stored on the best gpu format
                lInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
                lInfo.usage = UsageFlags;

                return lInfo;
            }

            /**
             * Constructs and returns a VkImageViewCreateInfo structure, which is used to specify parameters for creating a Vulkan image view.
             * This function is typically used to create a 2D image view for a specified image, format, and aspect configuration.
             *
             * @param Format The format of the image view. Specifies how the image data will be interpreted.
             * @param Image The Vulkan image handle to associate with the image view.
             * @param AspectFlags A bitmask of VkImageAspectFlags defining which aspects of the image will be included in the view (e.g., color, depth, stencil).
             * @return A configured VkImageViewCreateInfo structure ready for image view creation.
             */
            FORCEINLINE VkImageViewCreateInfo ImageviewCreateInfo(VkFormat Format, VkImage Image, VkImageAspectFlags AspectFlags)
            {
                // build a image-view for the depth image to use for rendering
                VkImageViewCreateInfo lInfo = {};
                lInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                lInfo.pNext = nullptr;

                lInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                lInfo.image = Image;
                lInfo.format = Format;
                lInfo.subresourceRange.baseMipLevel = 0;
                lInfo.subresourceRange.levelCount = 1;
                lInfo.subresourceRange.baseArrayLayer = 0;
                lInfo.subresourceRange.layerCount = 1;
                lInfo.subresourceRange.aspectMask = AspectFlags;

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

            /**
             * Copies image data from a source image to a destination image using specified extents and regions.
             * This function utilizes `vkCmdBlitImage2` for blitting operations, including scaling if necessary.
             * Vulkan has 2 main ways of copying one image to another. you can use VkCmdCopyImage or VkCmdBlitImage.
             * CopyImage is faster, but its much more restricted.
             * @param CommandBuffer The command buffer to record the blit operation onto. It must be in a recording state.
             * @param ImgSource The source Vulkan image that contains the data to be copied.
             * @param ImgDestination The destination Vulkan image to which the data will be copied.
             * @param SourceSize The dimensions of the source image region to be copied.
             * @param DestinationSize The dimensions of the destination image region where data will be copied.
             */
            static void CopyImageToImage(VkCommandBuffer CommandBuffer, VkImage ImgSource, VkImage ImgDestination, VkExtent2D SourceSize, VkExtent2D DestinationSize)
            {
                VkImageBlit2 lBlitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

                lBlitRegion.srcOffsets[1].x = SourceSize.width;
                lBlitRegion.srcOffsets[1].y = SourceSize.height;
                lBlitRegion.srcOffsets[1].z = 1;

                lBlitRegion.dstOffsets[1].x = DestinationSize.width;
                lBlitRegion.dstOffsets[1].y = DestinationSize.height;
                lBlitRegion.dstOffsets[1].z = 1;

                lBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                lBlitRegion.srcSubresource.baseArrayLayer = 0;
                lBlitRegion.srcSubresource.layerCount = 1;
                lBlitRegion.srcSubresource.mipLevel = 0;

                lBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                lBlitRegion.dstSubresource.baseArrayLayer = 0;
                lBlitRegion.dstSubresource.layerCount = 1;
                lBlitRegion.dstSubresource.mipLevel = 0;

                VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
                blitInfo.dstImage = ImgDestination;
                blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                blitInfo.srcImage = ImgSource;
                blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                blitInfo.filter = VK_FILTER_LINEAR;
                blitInfo.regionCount = 1;
                blitInfo.pRegions = &lBlitRegion;

                vkCmdBlitImage2(CommandBuffer, &blitInfo);
            }

            static void GenerateMipmaps(VkCommandBuffer CommandBuffer, VkImage Image, VkExtent2D ImageSize) {}

            FORCEINLINE VkRenderingAttachmentInfo AttachmentInfo(VkImageView View, const VkClearValue* Clear, VkImageLayout Layout /*= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL*/)
            {
                VkRenderingAttachmentInfo lColorAttachment {};
                lColorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                lColorAttachment.pNext = nullptr;

                lColorAttachment.imageView = View;
                lColorAttachment.imageLayout = Layout;
                lColorAttachment.loadOp = Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
                lColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

                if (Clear)
                {
                    lColorAttachment.clearValue = *Clear;
                }

                return lColorAttachment;
            }

            FORCEINLINE VkRenderingInfo RenderingInfo(VkExtent2D RenderExtent, const VkRenderingAttachmentInfo* ColorAttachment, const VkRenderingAttachmentInfo* DepthAttachment)
            {
                VkRenderingInfo lRenderInfo {};
                lRenderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
                lRenderInfo.pNext = nullptr;

                lRenderInfo.renderArea = VkRect2D { VkOffset2D { 0, 0 }, RenderExtent };
                lRenderInfo.layerCount = 1;
                lRenderInfo.colorAttachmentCount = 1;
                lRenderInfo.pColorAttachments = ColorAttachment;
                lRenderInfo.pDepthAttachment = DepthAttachment;
                lRenderInfo.pStencilAttachment = nullptr;

                return lRenderInfo;
            }
        }
    }
}
