#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanRenderer.h"

#include <VkBootstrap.h>

#include "Opaax/Math/OpaaxMathMacro.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKGlobal.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKHelper.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanMacro.h"
#include "Opaax/Window/OpaaxWindow.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

using namespace OPAAX::RENDERER::VULKAN;

void OpaaxVulkanRenderer::CreateVulkanSurface()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Creating the vulkan surface...")
    
    SDL_Vulkan_CreateSurface(reinterpret_cast<SDL_Window*>(GetOpaaxWindow()->GetNativeWindow()), m_vkInstance, nullptr, &m_vkSurface);

    if(m_vkSurface == VK_NULL_HANDLE)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer]: Failed to create Vulkan Surface!")
        throw std::runtime_error("Failed to create Vulkan Surface!");
    }
    
    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan surface created!")
}

void OpaaxVulkanRenderer::InitVulkanSwapchain()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Initializing Vulkan swapchain...")
    
    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);

    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan swapchain initialized!")
}

void OpaaxVulkanRenderer::InitVulkanCommands()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Initializing Vulkan Commands...")

    /*
     * We are sending VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
     * which tells Vulkan that we expect to be able to reset individual command buffers made from that pool.
     */
    VkCommandPoolCreateInfo lCommandPoolInfo = VULKAN_HELPER::CommandPoolCreateInfo(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for(int i = 0; i < VULKAN_CONST::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkCreateCommandPool(m_vkDevice, &lCommandPoolInfo, nullptr, &m_framesData[i].CommandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo lCmdAllocInfo = VULKAN_HELPER::CommandBufferAllocateInfo(m_framesData[i].CommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_vkDevice, &lCmdAllocInfo, &m_framesData[i].MainCommandBuffer));
    }

    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan Commands initialized!")
}

void OpaaxVulkanRenderer::InitVulkanSyncs()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Initializing Vulkan Syncs objects...")
    
    //create syncronization structures
    //one fence to control when the gpu has finished rendering the frame,
    //and 2 semaphores to synchronize rendering with swapchain
    //we want the fence to start signalled so we can wait on it on the first frame
    VkFenceCreateInfo lFenceCreateInfo = VULKAN_HELPER::FenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
    VkSemaphoreCreateInfo lSemaphoreCreateInfo = VULKAN_HELPER::SemaphoreCreateInfo();

    for (int i = 0; i < VULKAN_CONST::MAX_FRAMES_IN_FLIGHT; i++)
    {
        VK_CHECK(vkCreateFence(m_vkDevice, &lFenceCreateInfo, nullptr, &m_framesData[i].RenderFence));
        VK_CHECK(vkCreateSemaphore(m_vkDevice, &lSemaphoreCreateInfo, nullptr, &m_framesData[i].SwapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(m_vkDevice, &lSemaphoreCreateInfo, nullptr, &m_framesData[i].RenderSemaphore));
    }

    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan Syncs objects initialized!")
}

void OpaaxVulkanRenderer::CreateSwapchain(UInt32 Width, UInt32 Height)
{
    vkb::SwapchainBuilder lSwapchainBuilder{ m_vkPhysicalDevice, m_vkDevice, m_vkSurface };

    m_vkSwapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain lVKBSwapchain = lSwapchainBuilder
        //.use_default_format_selection()
        .set_desired_format(VkSurfaceFormatKHR{ .format = m_vkSwapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        //use vsync present mode
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(Width, Height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    m_vkSwapchainExtent = lVKBSwapchain.extent;
    
    //store swapchain and its related images
    m_vkSwapchain = lVKBSwapchain.swapchain;
    m_vkSwapchainImages = lVKBSwapchain.get_images().value();
    m_vkSwapchainImageViews = lVKBSwapchain.get_image_views().value();
}

void OpaaxVulkanRenderer::DestroySwapchain()
{
    vkDestroySwapchainKHR(m_vkDevice, m_vkSwapchain, nullptr);

    // Destroy swapchain resources
    for (int i = 0; i < m_vkSwapchainImageViews.size(); i++)
    {
        vkDestroyImageView(m_vkDevice, m_vkSwapchainImageViews[i], nullptr);
    }
}

OpaaxVulkanRenderer::OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window):IOpaaxRendererContext(Window)
{
    m_windowExtent.width = Window->GetWidth();
    m_windowExtent.height = Window->GetHeight();
}

OpaaxVulkanRenderer::~OpaaxVulkanRenderer()
{
    IOpaaxRendererContext::~IOpaaxRendererContext();
}

void OpaaxVulkanRenderer::InitVulkanBootStrap()
{
    OPAAX_LOG("[OpaaxVulkanRenderer]: Building VK Instance and Validation Layer (if enable)...")
    
    vkb::InstanceBuilder lBuilder;

    //Debugger
    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        lBuilder.request_validation_layers(VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
        .enable_validation_layers(VULKAN_CONST::G_VALIDATION_LAYERS.data())
        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        .set_debug_messenger_type(VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT);
    }
    else
    {
        lBuilder.request_validation_layers(VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS);
    }

    auto lExtensions = VULKAN_HELPER::GetRequiredExtensions();
    
    //make the vulkan instance, with basic debug features
    auto lBuildResult = lBuilder.set_app_name(OPAAX_CONST::ENGINE_NAME.c_str())
        .set_debug_callback(&VULKAN_HELPER::DebugCallback)
        .require_api_version(1, 3, 0)
        .set_engine_name(OPAAX_CONST::ENGINE_NAME.c_str())
        .set_engine_version(1, 0, 0)
        .enable_extensions(static_cast<UInt32>(lExtensions.size()), lExtensions.data())
        .build();

    
    vkb::Instance lVkbInstance = lBuildResult.value();

    //grab the instance 
    m_vkInstance = lVkbInstance.instance;
    //grab the debug messenger 
    m_vkDebugMessenger = lVkbInstance.debug_messenger;

    if(m_vkInstance == VK_NULL_HANDLE)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer]: Failed to create Vulkan Instance!")
        throw std::runtime_error("Failed to create instance!");
    }

    if(VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS && m_vkDebugMessenger == VK_NULL_HANDLE)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer]: Failed to create Vulkan debug messenger!")
        throw std::runtime_error("Failed to create Vulkan debug messenger!");
    }

    OPAAX_LOG("[OpaaxVulkanRenderer]: VK Instance and Validation Layer Built!")
    
    CreateVulkanSurface();

    OPAAX_LOG("[OpaaxVulkanRenderer]: Start Picking GPU...")

    //vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features lFeatures13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    lFeatures13.dynamicRendering = true;
    lFeatures13.synchronization2 = true;

    //vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features lFeatures12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    lFeatures12.bufferDeviceAddress = true;
    lFeatures12.descriptorIndexing = true;


    //use vkbootstrap to select a gpu. 
    //We want a gpu that can write to the SDL surface and supports vulkan 1.3 with the correct features
    vkb::PhysicalDeviceSelector lGPUSelector{ lVkbInstance, m_vkSurface };
    vkb::PhysicalDevice lVKBPhysicalDevice = lGPUSelector
        .set_minimum_version(1, 3)
        .set_required_features_13(lFeatures13)
        .set_required_features_12(lFeatures12)
        .select()
        .value();

    m_vkPhysicalDevice = lVKBPhysicalDevice.physical_device;

    if (m_vkPhysicalDevice == VK_NULL_HANDLE)
    {
        OPAAX_ERROR("[OpaaxVulkanRenderer] Failed to find a suitable GPU!")
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties lProps;
    vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &lProps);

    OPAAX_LOG("[OpaaxVulkanRenderer]: Picked GPU: %1% !!", %lProps.deviceName)
    OPAAX_LOG("[OpaaxVulkanRenderer]: End Start Picking GPU!")

    OPAAX_LOG("[OpaaxVulkanRenderer]: Creating Device...")

    //create the final vulkan device
    vkb::DeviceBuilder lVKBDeviceBuilder{ lVKBPhysicalDevice };

    vkb::Device lVKBDevice = lVKBDeviceBuilder.build().value();

    // Get the VkDevice handle used in the rest of a vulkan application
    m_vkDevice = lVKBDevice.device;


    // use vkbootstrap to get a Graphics queue
    m_vkGraphicsQueue       = lVKBDevice.get_queue(vkb::QueueType::graphics).value();
    m_graphicsQueueFamily   = lVKBDevice.get_queue_index(vkb::QueueType::graphics).value();
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Init =======================")

    InitVulkanBootStrap();
    InitVulkanSwapchain();
    InitVulkanCommands();
    InitVulkanSyncs();
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan End Init =======================")
    return false;
}

void OpaaxVulkanRenderer::Resize() {}
void OpaaxVulkanRenderer::RenderFrame()
{
    const OpaaxVKFrameData& lCurrFrameData = GetCurrentFrameData();
    
    // wait until the gpu has finished rendering the last frame.
    VK_CHECK(vkWaitForFences(m_vkDevice, 1, &lCurrFrameData.RenderFence, true, OP_UMAX_64));
    VK_CHECK(vkResetFences(m_vkDevice, 1, &lCurrFrameData.RenderFence));

    //request image from the swapchain
    UInt32 lSwapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(m_vkDevice, m_vkSwapchain, 1000000000,
        lCurrFrameData.SwapchainSemaphore, nullptr, &lSwapchainImageIndex));

    VkCommandBuffer lCommandBuffer = lCurrFrameData.MainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(lCommandBuffer, 0));

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo lCommandBufferBeginInfo = VULKAN_HELPER::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    //start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lCommandBufferBeginInfo));

    //@https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts
    //make the swapchain image into writeable mode before rendering
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_vkSwapchainImages[lSwapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue lClearValue;
    float flash = std::abs(std::sin(m_frameNumber / 120.f));
    lClearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = VULKAN_HELPER::ImageSubResourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

    //clear image
    vkCmdClearColorImage(lCommandBuffer, m_vkSwapchainImages[lSwapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &lClearValue, 1, &clearRange);

    //make the swapchain image into presentable mode
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_vkSwapchainImages[lSwapchainImageIndex],VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(lCommandBuffer));

    //prepare the submission to the queue. 
    //we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the _renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo lCommandBufferSubmitInfo = VULKAN_HELPER::CommandBufferSubmitInfo(lCommandBuffer);	

    VkSemaphoreSubmitInfo lSemaphoreWaitInfo = VULKAN_HELPER::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrameData().SwapchainSemaphore);
    VkSemaphoreSubmitInfo lSemaphoreSignalInfo = VULKAN_HELPER::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrameData().RenderSemaphore);	

    VkSubmitInfo2 lSubmitInfo = VULKAN_HELPER::SubmitInfo(&lCommandBufferSubmitInfo,&lSemaphoreSignalInfo,&lSemaphoreWaitInfo);	

    //submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_vkGraphicsQueue, 1, &lSubmitInfo, GetCurrentFrameData().RenderFence));

    //prepare present
    // this will put the image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that, 
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR lPresentInfo = {};
    lPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    lPresentInfo.pNext = nullptr;
    lPresentInfo.pSwapchains = &m_vkSwapchain;
    lPresentInfo.swapchainCount = 1;

    lPresentInfo.pWaitSemaphores = &GetCurrentFrameData().RenderSemaphore;
    lPresentInfo.waitSemaphoreCount = 1;

    lPresentInfo.pImageIndices = &lSwapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(m_vkGraphicsQueue, &lPresentInfo));

    GetCurrentFrameData().DeletionQueue.Flush();
    
    //increase the number of frames drawn
    m_frameNumber++;
}
void OpaaxVulkanRenderer::Shutdown()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down =======================")

    //make sure the gpu has stopped doing its things
    vkDeviceWaitIdle(m_vkDevice);

    for(int i = 0; i < VULKAN_CONST::MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyCommandPool(m_vkDevice, m_framesData[i].CommandPool, nullptr);

        //destroy sync objects
        vkDestroyFence(m_vkDevice, m_framesData[i].RenderFence, nullptr);
        vkDestroySemaphore(m_vkDevice, m_framesData[i].RenderSemaphore, nullptr);
        vkDestroySemaphore(m_vkDevice ,m_framesData[i].SwapchainSemaphore, nullptr);

        m_framesData[i].DeletionQueue.Flush();
    }

    DestroySwapchain();
    
    vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
    vkDestroyDevice(m_vkDevice, nullptr);
    
    if (VULKAN_CONST::G_ENABLE_VALIDATION_LAYERS)
    {
        vkb::destroy_debug_utils_messenger(m_vkInstance, m_vkDebugMessenger);
    }
    
    vkDestroyInstance(m_vkInstance, nullptr);
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down End =======================")
}
