#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVulkanRenderer.h"

#include <VkBootstrap.h>

#include "Opaax/Renderer/Vulkan/OpaaxVKGlobal.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKHelper.h"
#include "Opaax/Window/OpaaxWindow.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

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
    CreateSwapchain(m_windowExtent.width, m_windowExtent.height);
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
}

bool OpaaxVulkanRenderer::Initialize()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Init =======================")

    InitVulkanBootStrap();
    InitVulkanSwapchain();
    
    OPAAX_VERBOSE("======================= Renderer - Vulkan End Init =======================")
    return false;
}

void OpaaxVulkanRenderer::Resize() {}
void OpaaxVulkanRenderer::RenderFrame() {}
void OpaaxVulkanRenderer::Shutdown()
{
    OPAAX_VERBOSE("======================= Renderer - Vulkan Shutting Down =======================")

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
