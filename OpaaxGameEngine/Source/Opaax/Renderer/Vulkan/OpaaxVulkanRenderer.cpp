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
#include "Opaax/Renderer/Vulkan/OpaaxVKDescriptorLayoutBuilder.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKPipelineBuilder.h"

//IMGUi
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_vulkan.h"
#include "Opaax/OpaaxEngine.h"
#include "Opaax/Imgui/OpaaxImguiBase.h"
#include "Opaax/Imgui/OpaaxImguiVulkan.h"
#include "Opaax/Renderer/OpaaxShaderTypes.h"

using namespace OPAAX::RENDERER::VULKAN;

OpaaxVulkanRenderer::OpaaxVulkanRenderer(OPAAX::OpaaxWindow* const Window): IOpaaxRendererContext(Window),
                                                                            m_vkSwapchainImageFormat(),
                                                                            m_vkSwapchainExtent(),
                                                                            m_vmaAllocator(nullptr),
                                                                            m_drawExtent(),
                                                                            m_globalDescriptorAllocator(),
                                                                            m_vkDrawImageDescriptors(nullptr),
                                                                            m_vkDrawImageDescriptorLayout(nullptr),
                                                                            m_gradientPipelineLayout(nullptr)
{
    m_windowExtent.width = Window->GetWidth();
    m_windowExtent.height = Window->GetHeight();
}

OpaaxVulkanRenderer::~OpaaxVulkanRenderer()
{
    IOpaaxRendererContext::~IOpaaxRendererContext();
}

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

    //draw image size will match the window
    VkExtent3D lDrawImageExtent = {
        m_windowExtent.width,
        m_windowExtent.height,
        1
    };

    //hardcoding the draw format to 32-bit float
    m_drawImage.ImageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    m_drawImage.ImageExtent = lDrawImageExtent;

    VkImageUsageFlags lDrawImageUsages{};
    lDrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    lDrawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    lDrawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    lDrawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo lRenderImgInfo = VULKAN_HELPER::ImageCreateInfo(m_drawImage.ImageFormat, lDrawImageUsages, lDrawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo lRenderImgAllocationInfo = {};
    lRenderImgAllocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    lRenderImgAllocationInfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(m_vmaAllocator, &lRenderImgInfo, &lRenderImgAllocationInfo, &m_drawImage.Image, &m_drawImage.Allocation, nullptr);

    //build an image-view for the draw image to use for rendering
    VkImageViewCreateInfo lRenderViewInfo = VULKAN_HELPER::ImageviewCreateInfo(m_drawImage.ImageFormat, m_drawImage.Image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(m_vkDevice, &lRenderViewInfo, nullptr, &m_drawImage.ImageView));

    //add to deletion queues
    m_mainDeletionQueue.PushFunction([this]() {
        vkDestroyImageView(m_vkDevice, m_drawImage.ImageView, nullptr);
        vmaDestroyImage(m_vmaAllocator, m_drawImage.Image, m_drawImage.Allocation);
    });

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

    //Immediate Submit
    {
        VK_CHECK(vkCreateCommandPool(m_vkDevice, &lCommandPoolInfo, nullptr, &m_immediateCommandPool));

        // allocate the command buffer for immediate submits
        VkCommandBufferAllocateInfo lCommandAllocInfo = VULKAN_HELPER::CommandBufferAllocateInfo(m_immediateCommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(m_vkDevice, &lCommandAllocInfo, &m_immediateCommandBuffer));

        m_mainDeletionQueue.PushFunction([this]() { 
            vkDestroyCommandPool(m_vkDevice, m_immediateCommandPool, nullptr);
        });
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

    //Immediate submit
    {
        VK_CHECK(vkCreateFence(m_vkDevice, &lFenceCreateInfo, nullptr, &m_immediateFence));
        m_mainDeletionQueue.PushFunction([this]() { vkDestroyFence(m_vkDevice, m_immediateFence, nullptr); });
    }

    OPAAX_LOG("[OpaaxVulkanRenderer]: Vulkan Syncs objects initialized!")
}

void OpaaxVulkanRenderer::InitDescriptors()
{
    //create a descriptor pool that will hold 10 sets with 1 image each
    std::vector<OpaaxVKDescriptorAllocator::OpaaxPoolSizeRatio> lSizes =
    {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 }
    };

    m_globalDescriptorAllocator.InitPool(m_vkDevice, 10, lSizes);

    //make the descriptor set layout for our compute draw
    {
        OpaaxVKDescriptorLayoutBuilder lBuilder;
        lBuilder.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        m_vkDrawImageDescriptorLayout = lBuilder.Build(m_vkDevice, VK_SHADER_STAGE_COMPUTE_BIT | VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    //allocate a descriptor set for our draw image
    m_vkDrawImageDescriptors = m_globalDescriptorAllocator.Allocate(m_vkDevice, m_vkDrawImageDescriptorLayout);	

    VkDescriptorImageInfo lImgInfo{};
    lImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    lImgInfo.imageView = m_drawImage.ImageView;
	
    VkWriteDescriptorSet lDrawImageWrite = {};
    lDrawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    lDrawImageWrite.pNext = nullptr;
	
    lDrawImageWrite.dstBinding = 0;
    lDrawImageWrite.dstSet = m_vkDrawImageDescriptors;
    lDrawImageWrite.descriptorCount = 1;
    lDrawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    lDrawImageWrite.pImageInfo = &lImgInfo;

    vkUpdateDescriptorSets(m_vkDevice, 1, &lDrawImageWrite, 0, nullptr);

    //make sure both the descriptor allocator and the new layout get cleaned up properly
    m_mainDeletionQueue.PushFunction([&]() {
        m_globalDescriptorAllocator.DestroyPool(m_vkDevice);
        vkDestroyDescriptorSetLayout(m_vkDevice, m_vkDrawImageDescriptorLayout, nullptr);
    });
}

void OpaaxVulkanRenderer::InitPipelines()
{
    InitBackgroundPipelines();
}

void OpaaxVulkanRenderer::InitBackgroundPipelines()
{
    VkPipelineLayoutCreateInfo lComputeLayout{};
    lComputeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lComputeLayout.pNext = nullptr;
    lComputeLayout.pSetLayouts = &m_vkDrawImageDescriptorLayout;
    lComputeLayout.setLayoutCount = 1;

    VkPushConstantRange lPushConstant{};
    lPushConstant.offset = 0;
    lPushConstant.size = sizeof(SHADER::OpaaxComputeShaderPushConstants) ;
    lPushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    lComputeLayout.pPushConstantRanges = &lPushConstant;
    lComputeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(m_vkDevice, &lComputeLayout, nullptr, &m_gradientPipelineLayout));

    VkShaderModule lComputeDrawShader;
    if (!VULKAN_HELPER::LoadShaderModule("Shaders/GradientColor.comp.spv", m_vkDevice, &lComputeDrawShader))
    {
        OPAAX_ERROR("Failed to load shaders!")
    }

    VkShaderModule lSkyShader;
    if (!VULKAN_HELPER::LoadShaderModule("Shaders/Sky.comp.spv", m_vkDevice, &lSkyShader))
    {
        OPAAX_ERROR("Failed to load shaders!")
    }

    VkPipelineShaderStageCreateInfo lStageinfo{};
    lStageinfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lStageinfo.pNext = nullptr;
    lStageinfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    lStageinfo.module = lComputeDrawShader;
    lStageinfo.pName = "main";

    VkComputePipelineCreateInfo lComputePipelineCreateInfo{};
    lComputePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    lComputePipelineCreateInfo.pNext = nullptr;
    lComputePipelineCreateInfo.layout = m_gradientPipelineLayout;
    lComputePipelineCreateInfo.stage = lStageinfo;

    OpaaxVKComputeEffect lGradient;
    lGradient.Layout = m_gradientPipelineLayout;
    lGradient.Name = "GradientColor";
    lGradient.Data = {};

    //default colors
    lGradient.Data.Data1 = glm::vec4(1, 0, 0, 1);
    lGradient.Data.Data2 = glm::vec4(0, 0, 1, 1);
	
    VK_CHECK(vkCreateComputePipelines(m_vkDevice,VK_NULL_HANDLE,1,&lComputePipelineCreateInfo, nullptr, &lGradient.Pipeline));

    //change the shader module only to create the sky shader
    lComputePipelineCreateInfo.stage.module = lSkyShader;

    OpaaxVKComputeEffect lSky;
    lSky.Layout = m_gradientPipelineLayout;
    lSky.Name = "sky";
    lSky.Data = {};
    //default sky parameters
    lSky.Data.Data1 = glm::vec4(0.1, 0.2, 0.4 ,0.97);

    VK_CHECK(vkCreateComputePipelines(m_vkDevice, VK_NULL_HANDLE, 1, &lComputePipelineCreateInfo, nullptr, &lSky.Pipeline));

    //add the 2 background effects into the array
    m_backgroundEffects.push_back(lGradient);
    m_backgroundEffects.push_back(lSky);
    
    vkDestroyShaderModule(m_vkDevice, lComputeDrawShader, nullptr);
    vkDestroyShaderModule(m_vkDevice, lSkyShader, nullptr);

    m_mainDeletionQueue.PushFunction([this]() {
        vkDestroyPipelineLayout(m_vkDevice, m_gradientPipelineLayout, nullptr);

        for (auto& lEffect : m_backgroundEffects)
        {
            vkDestroyPipeline(m_vkDevice, lEffect.Pipeline, nullptr);
        }
    });
}

void OpaaxVulkanRenderer::InitImgui()
{
    SDL_Window* lWindow = static_cast<SDL_Window*>(GetOpaaxWindow()->GetNativeWindow());
    IMGUI::OpaaxImguiVulkan* lImguiReintepreted = static_cast<IMGUI::OpaaxImguiVulkan*>(&OpaaxEngine::Get().GetImgui());
    lImguiReintepreted->Initialize(lWindow, m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_vkGraphicsQueue, m_vkSwapchainImageFormat);
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

void OpaaxVulkanRenderer::DrawBackground(VkCommandBuffer CommandBuffer)
{
    // //make a clear-color from frame number. This will flash with a 120 frame period.
    // VkClearColorValue lClearValue;
    // float flash = std::abs(std::sin(m_frameNumber / 120.f));
    // lClearValue = { { 0.0f, 0.0f, flash, 1.0f } };
    //
    // VkImageSubresourceRange lClearRange = VULKAN_HELPER::ImageSubResourceRange(VK_IMAGE_ASPECT_COLOR_BIT);
    //
    // //clear image
    // vkCmdClearColorImage(CommandBuffer, m_drawImage.Image, VK_IMAGE_LAYOUT_GENERAL, &lClearValue, 1, &lClearRange);

    // // bind the gradient drawing compute pipeline
    // vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipeline);
    //
    // // bind the descriptor set containing the draw image for the compute pipeline
    // vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipelineLayout, 0, 1, &m_vkDrawImageDescriptors, 0, nullptr);
    //
    // // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    // vkCmdDispatch(CommandBuffer, std::ceil(m_drawExtent.width / 16.0), std::ceil(m_drawExtent.height / 16.0), 1);

    // bind the gradient drawing compute pipeline
    //vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipeline);

    // // bind the descriptor set containing the draw image for the compute pipeline
    // vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipelineLayout, 0, 1, &m_vkDrawImageDescriptors, 0, nullptr);
    //
    // SHADER::OpaaxComputeShaderPushConstants lPushConst;
    // lPushConst.data1 = glm::vec4(1, 0, 0, 1);
    // lPushConst.data2 = glm::vec4(0, 0, 1, 1);
    //
    // vkCmdPushConstants(CommandBuffer, m_gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SHADER::OpaaxComputeShaderPushConstants), &lPushConst);
    // // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    // vkCmdDispatch(CommandBuffer, std::ceil(m_drawExtent.width / 16.0), std::ceil(m_drawExtent.height / 16.0), 1);

    OpaaxVKComputeEffect& lEffect = m_backgroundEffects[m_currentBackgroundEffect];

    // bind the background compute pipeline
    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, lEffect.Pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_gradientPipelineLayout, 0, 1, &m_vkDrawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(CommandBuffer, m_gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SHADER::OpaaxComputeShaderPushConstants), &lEffect.Data);
    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(CommandBuffer, std::ceil(m_drawExtent.width / 16.0), std::ceil(m_drawExtent.height / 16.0), 1);
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

void OpaaxVulkanRenderer::ImmediateSubmit(OPSTDFunc<void(VkCommandBuffer CommandBuffer)>&& Function)
{
    VK_CHECK(vkResetFences(m_vkDevice, 1, &m_immediateFence));
    VK_CHECK(vkResetCommandBuffer(m_immediateCommandBuffer, 0));

    VkCommandBuffer lCommandBuffer = m_immediateCommandBuffer;

    VkCommandBufferBeginInfo lCommandBufferBeginInfo = VULKAN_HELPER::CommandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lCommandBufferBeginInfo));

    Function(lCommandBuffer);

    VK_CHECK(vkEndCommandBuffer(lCommandBuffer));

    VkCommandBufferSubmitInfo lCommandBufferInfo = VULKAN_HELPER::CommandBufferSubmitInfo(lCommandBuffer);
    VkSubmitInfo2 lSubmit = VULKAN_HELPER::SubmitInfo(&lCommandBufferInfo, nullptr, nullptr);

    // submit command buffer to the queue and execute it.
    // m_renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(m_vkGraphicsQueue, 1, &lSubmit, m_immediateFence));
    VK_CHECK(vkWaitForFences(m_vkDevice, 1, &m_immediateFence, true, OP_UMAX_64));
}

void OpaaxVulkanRenderer::DrawImgui(VkCommandBuffer CommandBuffer, VkImageView TargetImageView)
{
    VkRenderingAttachmentInfo lColorAttachment = VULKAN_HELPER::AttachmentInfo(TargetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo lRenderInfo = VULKAN_HELPER::RenderingInfo(m_vkSwapchainExtent, &lColorAttachment, nullptr);

    vkCmdBeginRendering(CommandBuffer, &lRenderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), CommandBuffer);

    vkCmdEndRendering(CommandBuffer);
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

void OpaaxVulkanRenderer::InitVMAAllocator()
{
    // initialize the memory allocator
    VmaAllocatorCreateInfo lAllocatorInfo = {};
    lAllocatorInfo.physicalDevice = m_vkPhysicalDevice;
    lAllocatorInfo.device = m_vkDevice;
    lAllocatorInfo.instance = m_vkInstance;
    lAllocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&lAllocatorInfo, &m_vmaAllocator);

    m_mainDeletionQueue.PushFunction([&]() { vmaDestroyAllocator(m_vmaAllocator); });
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Init =======================")

    InitVulkanBootStrap();
    InitVMAAllocator();
    InitVulkanSwapchain();
    InitVulkanCommands();
    InitVulkanSyncs();
    InitDescriptors();
    InitPipelines();
    InitImgui();
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan End Init =======================")
    return false;
}

void OpaaxVulkanRenderer::Resize() {}
void OpaaxVulkanRenderer::DrawImgui()
{
    if (ImGui::Begin("background"))
    {
        OpaaxVKComputeEffect& lEffectSelected = m_backgroundEffects[m_currentBackgroundEffect];
		
        ImGui::Text("Selected effect: ", lEffectSelected.Name);
		
        ImGui::SliderInt("Effect Index", &m_currentBackgroundEffect,0, m_backgroundEffects.size() - 1);
		
        ImGui::ColorEdit4("Data1", reinterpret_cast<float*>(&lEffectSelected.Data.Data1));
        ImGui::ColorEdit4("Data2", reinterpret_cast<float*>(&lEffectSelected.Data.Data2));
        
        ImGui::End();
    }
}

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

    m_drawExtent.width = m_drawImage.ImageExtent.width;
    m_drawExtent.height = m_drawImage.ImageExtent.height;

    //start the command buffer recording
    VK_CHECK(vkBeginCommandBuffer(lCommandBuffer, &lCommandBufferBeginInfo));

    //@https://docs.vulkan.org/spec/latest/chapters/resources.html#resources-image-layouts
    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_drawImage.Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    
    DrawBackground(lCommandBuffer);

    //transition the draw image and the swapchain image into their correct transfer layouts
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_drawImage.Image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_vkSwapchainImages[lSwapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    VULKAN_HELPER::CopyImageToImage(lCommandBuffer, m_drawImage.Image, m_vkSwapchainImages[lSwapchainImageIndex], m_drawExtent, m_vkSwapchainExtent);

    //draw imgui into the swapchain image
    DrawImgui(lCommandBuffer,  m_vkSwapchainImageViews[lSwapchainImageIndex]);
    
    // set swapchain image layout to Present so we can show it on the screen
    VULKAN_HELPER::TransitionImage(lCommandBuffer, m_vkSwapchainImages[lSwapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(lCommandBuffer));

    //prepare the submission to the queue. 
    //we want to wait on the m_presentSemaphore, as that semaphore is signaled when the swapchain is ready
    //we will signal the m_renderSemaphore, to signal that rendering has finished
    VkCommandBufferSubmitInfo lCommandBufferSubmitInfo = VULKAN_HELPER::CommandBufferSubmitInfo(lCommandBuffer);	

    VkSemaphoreSubmitInfo lSemaphoreWaitInfo = VULKAN_HELPER::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrameData().SwapchainSemaphore);
    VkSemaphoreSubmitInfo lSemaphoreSignalInfo = VULKAN_HELPER::SemaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrameData().RenderSemaphore);	

    VkSubmitInfo2 lSubmitInfo = VULKAN_HELPER::SubmitInfo(&lCommandBufferSubmitInfo,&lSemaphoreSignalInfo,&lSemaphoreWaitInfo);	

    //submit command buffer to the queue and execute it.
    //m_renderFence will now block until the graphic commands finish execution
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

    OpaaxEngine::Get().GetImgui().Shutdown();

    m_mainDeletionQueue.Flush();

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
