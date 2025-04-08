#include <set>
#include <Renderer/OpaaxVulkanContext.h>

#include <stdexcept>
#include "Core/OPLogMacro.h"

OpaaxVulkanContext::OpaaxVulkanContext(GLFWwindow* Window)
    : m_window(Window), m_vkInstance(VK_NULL_HANDLE), m_vkDebugMessenger(VK_NULL_HANDLE) {}

OpaaxVulkanContext::~OpaaxVulkanContext()
{
    Shutdown();
}

void OpaaxVulkanContext::CreateInstance()
{
    // Check if validation layers are enabled and supported, throw error if unsupported
    if (gEnableValidationLayers && !CheckValidationLayerSupport())
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Validation layers requested but not available!")
        throw std::runtime_error("Validation layers requested but not available!");
    }

    // Create and initialize Vulkan application info structure
    VkApplicationInfo lAppInfo{};
    lAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; // Struct type
    lAppInfo.pApplicationName = "OpaaxGameEngine"; // Application name
    lAppInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1); // Application version
    lAppInfo.pEngineName = "OpaaxEngine"; // Engine name
    lAppInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1); // Engine version
    lAppInfo.apiVersion = VK_API_VERSION_1_2; // Vulkan API version

    // Get required extensions from GLFW
    UInt32 lGlfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&lGlfwExtCount);

    // Collect required extensions into a vector
    std::vector<const char*> lExtensions(glfwExtensions, glfwExtensions + lGlfwExtCount);

    // Add debug extension if validation layers are enabled
    if (gEnableValidationLayers)
    {
        lExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Create and initialize Vulkan instance creation info structure
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; // Struct type
    createInfo.pApplicationInfo = &lAppInfo; // Pointer to application info struct
    createInfo.enabledExtensionCount = static_cast<uint32_t>(lExtensions.size()); // Number of extensions
    createInfo.ppEnabledExtensionNames = lExtensions.data(); // List of extensions

    // Handle validation layers
    if (gEnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(gValidationLayers.size()); // Number of validation layers
        createInfo.ppEnabledLayerNames = gValidationLayers.data(); // List of validation layers
    }
    else
    {
        createInfo.enabledLayerCount = 0; // No validation layers
    }

    // Attempt to create Vulkan instance and throw error if it fails
    if (vkCreateInstance(&createInfo, nullptr, &m_vkInstance) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create Vulkan instance.")
        throw std::runtime_error("Failed to create Vulkan instance.");
    }
}

void OpaaxVulkanContext::SetupDebugMessenger()
{
    if (!gEnableValidationLayers)
    {
        // If validation layers are not enabled, exit the function
        return;
    }

    // Initialize the Vulkan debug messenger create info structure
    VkDebugUtilsMessengerCreateInfoEXT lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT; // Structure type
    lCreateInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | // Enable warning messages
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT; // Enable error messages

    lCreateInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | // General messages
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | // Validation messages
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT; // Performance messages

    // Callback function to handle validation layer messages
    lCreateInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT,
                                     VkDebugUtilsMessageTypeFlagsEXT,
                                     const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                     void*) -> VkBool32
    {
        // Log the validation layer message
        OPAAX_ERROR("Validation Layer: %1%", %pCallbackData->pMessage)
        return VK_FALSE; // Continue execution after the message
    };

    // Attempt to create the Vulkan debug messenger
    if (CreateDebugUtilsMessengerEXT(m_vkInstance, &lCreateInfo, nullptr, &m_vkDebugMessenger) != VK_SUCCESS)
    {
        // Throw an error if the debug messenger creation fails
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to set up debug messenger!")
        throw std::runtime_error("Failed to set up debug messenger!");
    }
}

bool OpaaxVulkanContext::CheckValidationLayerSupport()
{
    UInt32 lLayerCount;
    // Query the number of available Vulkan instance layers
    vkEnumerateInstanceLayerProperties(&lLayerCount, nullptr);

    // Retrieve the layer properties of all available Vulkan instance layers
    std::vector<VkLayerProperties> lAvailableLayers(lLayerCount);
    vkEnumerateInstanceLayerProperties(&lLayerCount, lAvailableLayers.data());

    // Loop through the validation layers we want to use (gValidationLayers)
    for (const char* lLayerName : gValidationLayers)
    {
        bool lbFound = false;
        // Check if the current validation layer is supported by the available layers
        for (const auto& layerProp : lAvailableLayers)
        {
            if (strcmp(lLayerName, layerProp.layerName) == 0) // Compare layer names
            {
                lbFound = true; // Validation layer is found
                break;
            }
        }
        // If any layer from gValidationLayers is not found, return false
        if (!lbFound)
        {
            return false;
        }
    }

    // All required validation layers are supported
    return true;
}

void OpaaxVulkanContext::CreateSurface()
{
    if (glfwCreateWindowSurface(m_vkInstance, m_window, nullptr, &m_vkSurface) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create window surface!")
        throw std::runtime_error("Failed to create window surface!");
    }
}

void OpaaxVulkanContext::PickPhysicalDevice()
{
    // Query the number of available physical devices
    UInt32 lDeviceCount = 0;
    vkEnumeratePhysicalDevices(m_vkInstance, &lDeviceCount, nullptr);

    // Check if there are any physical devices available
    if (lDeviceCount == 0)
    {
        // Log an error if no Vulkan-supported GPUs are found
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find GPUs with Vulkan support!")
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    // Retrieve the list of physical devices
    std::vector<VkPhysicalDevice> lDevices(lDeviceCount);
    vkEnumeratePhysicalDevices(m_vkInstance, &lDeviceCount, lDevices.data());

    // Iterate over the list of physical devices to find a suitable device
    for (const auto& lDevice : lDevices)
    {
        // Check if the current device meets the requirements
        if (IsDeviceSuitable(lDevice))
        {
            // Store the suitable device and stop searching
            m_vkPhysicalDevice = lDevice;
            break;
        }
    }

    // Verify if a suitable GPU was found
    if (m_vkPhysicalDevice == VK_NULL_HANDLE)
    {
        // Log an error if no suitable GPU was found
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find a suitable GPU!")
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(m_vkPhysicalDevice, &props);
    OPAAX_VERBOSE("[OpaaxVulkanContext] Picked GPU: %1%", %props.deviceName)
}

bool OpaaxVulkanContext::IsDeviceSuitable(VkPhysicalDevice Device)
{
    VkPhysicalDeviceProperties lDeviceProperties;
    vkGetPhysicalDeviceProperties(Device, &lDeviceProperties);

    VkPhysicalDeviceFeatures lDeviceFeatures;
    vkGetPhysicalDeviceFeatures(Device, &lDeviceFeatures);

    return lDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
        || lDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
}

QueueFamilyIndices OpaaxVulkanContext::FindQueueFamilies(VkPhysicalDevice Device)
{
    QueueFamilyIndices lIndices;

    UInt32 lQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, queueFamilies.data());

    // Evaluate each queue family
    for (UInt32 i = 0; i < lQueueFamilyCount; ++i)
    {
        const auto& queueFamily = queueFamilies[i];

        // Check for graphics support
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            lIndices.GraphicsFamily = i;
        }

        // Check for presentation support
        VkBool32 lPresentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, m_vkSurface, &lPresentSupport);

        if (lPresentSupport)
        {
            lIndices.PresentFamily = i;
        }

        if (lIndices.IsComplete())
        {
            break;
        }
    }

    return lIndices;
}

void OpaaxVulkanContext::CreateLogicalDevice()
{
    m_queueIndices = FindQueueFamilies(m_vkPhysicalDevice);

    if (!m_queueIndices.IsComplete())
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to find suitable queue families.")
        throw std::runtime_error("Queue families incomplete");
    }

    std::vector<VkDeviceQueueCreateInfo> lQueueCreateInfos;
    std::set<UInt32> lUniqueFamilies = {
        m_queueIndices.GraphicsFamily.value(),
        m_queueIndices.PresentFamily.value()
    };

    float lQueuePriority = 1.0f;
    for (uint32_t lQueueFamily : lUniqueFamilies)
    {
        VkDeviceQueueCreateInfo lQueueCreateInfo{};

        lQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        lQueueCreateInfo.queueFamilyIndex = lQueueFamily;
        lQueueCreateInfo.queueCount = 1;
        lQueueCreateInfo.pQueuePriorities = &lQueuePriority;
        lQueueCreateInfos.push_back(lQueueCreateInfo);
    }

    VkPhysicalDeviceFeatures lDeviceFeatures{}; // Fill later

    VkDeviceCreateInfo lCreateInfo{};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    lCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(lQueueCreateInfos.size());
    lCreateInfo.pQueueCreateInfos = lQueueCreateInfos.data();
    lCreateInfo.pEnabledFeatures = &lDeviceFeatures;

    // Enable swapchain extension
    lCreateInfo.enabledExtensionCount = static_cast<uint32_t>(gDeviceExtensions.size());
    lCreateInfo.ppEnabledExtensionNames = gDeviceExtensions.data();

    // Validation layers for logical device (legacy)
    if (gEnableValidationLayers)
    {
        lCreateInfo.enabledLayerCount = static_cast<UInt32>(gValidationLayers.size());
        lCreateInfo.ppEnabledLayerNames = gValidationLayers.data();
    }
    else
    {
        lCreateInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(m_vkPhysicalDevice, &lCreateInfo, nullptr, &m_vkDevice) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create logical device.")
        throw std::runtime_error("Failed to create logical device.");
    }

    // Retrieve queue handles
    vkGetDeviceQueue(m_vkDevice, m_queueIndices.GraphicsFamily.value(), 0, &m_vkGraphicsQueue);
    vkGetDeviceQueue(m_vkDevice, m_queueIndices.PresentFamily.value(), 0, &m_vkPresentQueue);

    OPAAX_VERBOSE("[OpaaxVulkanContext] Logical device and queues successfully created.")
}

void OpaaxVulkanContext::CreateSwapchain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupportForVK(m_vkPhysicalDevice);

    VkSurfaceFormatKHR lSurfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(swapChainSupport.presentModes);
    VkExtent2D lExtent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t lImageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && lImageCount > swapChainSupport.capabilities.maxImageCount)
    {
        lImageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_vkSurface;

    createInfo.minImageCount = lImageCount;
    createInfo.imageFormat = lSurfaceFormat.format;
    createInfo.imageColorSpace = lSurfaceFormat.colorSpace;
    createInfo.imageExtent = lExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamiliesForVK(m_vkPhysicalDevice);
    uint32_t queueFamilyIndices[] = {indices.GraphicsFamily.value(), indices.PresentFamily.value()};

    if (indices.GraphicsFamily != indices.PresentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_vkDevice, &createInfo, nullptr, &m_vkSwapchain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &lImageCount, nullptr);
    m_vkSwapchainImages.resize(lImageCount);
    vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &lImageCount, m_vkSwapchainImages.data());

    m_vkSwapchainImageFormat = lSurfaceFormat.format;
    m_vkSwapchainExtent = lExtent;
}

SwapChainSupportDetails OpaaxVulkanContext::QuerySwapChainSupportForVK(VkPhysicalDevice Device)
{
    SwapChainSupportDetails lDetails;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Device, m_vkSurface, &lDetails.capabilities);

    uint32_t lFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(Device, m_vkSurface, &lFormatCount, nullptr);

    if (lFormatCount != 0)
    {
        lDetails.formats.resize(lFormatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(Device, m_vkSurface, &lFormatCount, lDetails.formats.data());
    }

    uint32_t lPresentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(Device, m_vkSurface, &lPresentModeCount, nullptr);

    if (lPresentModeCount != 0)
    {
        lDetails.presentModes.resize(lPresentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(Device, m_vkSurface, &lPresentModeCount,
                                                  lDetails.presentModes.data());
    }

    return lDetails;
}

VkSurfaceFormatKHR OpaaxVulkanContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& Formats)
{
    for (const auto& lFormat : Formats)
    {
        if (lFormat.format == VK_FORMAT_B8G8R8A8_SRGB && lFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return lFormat;
        }
    }

    return Formats[0];
}

void OpaaxVulkanContext::CreateImageViews()
{
    m_vkSwapchainImageViews.resize(m_vkSwapchainImages.size());

    for (size_t i = 0; i < m_vkSwapchainImages.size(); i++)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_vkSwapchainImages[i];

        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_vkSwapchainImageFormat;

        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_vkDevice, &viewInfo, nullptr, &m_vkSwapchainImageViews[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanContext] Failed to create image view at index %1%", %static_cast<int>(i))
            throw std::runtime_error("Failed to create image views.");
        }
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Swapchain image views created.")
}

void OpaaxVulkanContext::CreateRenderPass()
{
    // Color attachment (swapchain image)
    VkAttachmentDescription lColorAttachment{};
    lColorAttachment.format = m_vkSwapchainImageFormat;
    lColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

    lColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // Clear on start
    lColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // Store result for presentation

    lColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    lColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    lColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    lColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Attachment reference for subpass
    VkAttachmentReference lColorAttachmentRef{};
    lColorAttachmentRef.attachment = 0;
    lColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Subpass definition
    VkSubpassDescription lSubpass{};
    lSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    lSubpass.colorAttachmentCount = 1;
    lSubpass.pColorAttachments = &lColorAttachmentRef;

    // Subpass dependency (for layout transitions)
    VkSubpassDependency lDependency{};
    lDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    lDependency.dstSubpass = 0;
    lDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    lDependency.srcAccessMask = 0;
    lDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    lDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // Create render pass
    VkRenderPassCreateInfo lRenderPassInfo{};
    lRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    lRenderPassInfo.attachmentCount = 1;
    lRenderPassInfo.pAttachments = &lColorAttachment;
    lRenderPassInfo.subpassCount = 1;
    lRenderPassInfo.pSubpasses = &lSubpass;
    lRenderPassInfo.dependencyCount = 1;
    lRenderPassInfo.pDependencies = &lDependency;

    if (vkCreateRenderPass(m_vkDevice, &lRenderPassInfo, nullptr, &m_vkRenderPass) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create render pass!")
        throw std::runtime_error("Failed to create render pass.");
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Render pass created.")
}

void OpaaxVulkanContext::CreateFrameBuffers()
{
    m_vkSwapchainFrameBuffers.resize(m_vkSwapchainImageViews.size());

    for (size_t i = 0; i < m_vkSwapchainImageViews.size(); i++)
    {
        VkImageView lAttachments[] = {m_vkSwapchainImageViews[i]};

        VkFramebufferCreateInfo lFramebufferInfo{};
        lFramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        lFramebufferInfo.renderPass = m_vkRenderPass;
        lFramebufferInfo.attachmentCount = 1;
        lFramebufferInfo.pAttachments = lAttachments;
        lFramebufferInfo.width = m_vkSwapchainExtent.width;
        lFramebufferInfo.height = m_vkSwapchainExtent.height;
        lFramebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_vkDevice, &lFramebufferInfo, nullptr, &m_vkSwapchainFrameBuffers[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanContext] Failed to create frame buffer for swapchain image index %1%",
                        %static_cast<int>(i))
            throw std::runtime_error("Failed to create frame buffers.");
        }
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Frame buffers created for all swapchain images.")
}

void OpaaxVulkanContext::CreateCommandPool()
{
    QueueFamilyIndices lIndices = FindQueueFamilies(m_vkPhysicalDevice);

    VkCommandPoolCreateInfo lPoolInfo{};
    lPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    lPoolInfo.queueFamilyIndex = lIndices.GraphicsFamily.value(); // Assuming std::optional<uint32_t>
    lPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(m_vkDevice, &lPoolInfo, nullptr, &m_vkCommandPool) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to create command pool.")
        throw std::runtime_error("Failed to create command pool.");
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Command pool created.")
}

void OpaaxVulkanContext::CreateCommandBuffers()
{
    m_vkCommandBuffers.resize(m_vkSwapchainFrameBuffers.size());

    VkCommandBufferAllocateInfo lAllocInfo{};
    lAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    lAllocInfo.commandPool = m_vkCommandPool;
    lAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    lAllocInfo.commandBufferCount = static_cast<uint32_t>(m_vkCommandBuffers.size());

    if (vkAllocateCommandBuffers(m_vkDevice, &lAllocInfo, m_vkCommandBuffers.data()) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to allocate command buffers.")
        throw std::runtime_error("Failed to allocate command buffers.");
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Allocated %1% command buffers.", %m_vkCommandBuffers.size())
}

void OpaaxVulkanContext::RecordCommandBuffers()
{
    for (size_t i = 0; i < m_vkCommandBuffers.size(); ++i)
    {
        VkCommandBufferBeginInfo lBeginInfo{};
        lBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        lBeginInfo.flags = 0; // Optional
        lBeginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(m_vkCommandBuffers[i], &lBeginInfo) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanContext] Failed to begin recording command buffer %1%.", %i)
            throw std::runtime_error("Failed to begin recording command buffer.");
        }

        VkClearValue lClearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};

        VkRenderPassBeginInfo lRenderPassInfo{};
        lRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        lRenderPassInfo.renderPass = m_vkRenderPass;
        lRenderPassInfo.framebuffer = m_vkSwapchainFrameBuffers[i];
        lRenderPassInfo.renderArea.offset = {0, 0};
        lRenderPassInfo.renderArea.extent = m_vkSwapchainExtent;
        lRenderPassInfo.clearValueCount = 1;
        lRenderPassInfo.pClearValues = &lClearColor;

        vkCmdBeginRenderPass(m_vkCommandBuffers[i], &lRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(m_vkCommandBuffers[i]);

        if (vkEndCommandBuffer(m_vkCommandBuffers[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanContext] Failed to record command buffer %1%.", %i)
            throw std::runtime_error("Failed to record command buffer.");
        }
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Recorded %1% command buffers.", %m_vkCommandBuffers.size())
}

void OpaaxVulkanContext::CreateSyncObjects()
{
    m_vkImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_vkRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_vkInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo lSemaphoreInfo{};
    lSemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // So we don’t wait on first frame

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(m_vkDevice, &lSemaphoreInfo, nullptr, &m_vkImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_vkDevice, &lSemaphoreInfo, nullptr, &m_vkRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &m_vkInFlightFences[i]) != VK_SUCCESS)
        {
            OPAAX_ERROR("[OpaaxVulkanContext] Failed to create synchronization objects for frame %1%!", %i)
            throw std::runtime_error("Failed to create sync objects!");
        }
    }

    OPAAX_VERBOSE("[OpaaxVulkanContext] Synchronization objects created for %1% frames in flight.", %MAX_FRAMES_IN_FLIGHT)
}

void OpaaxVulkanContext::DrawFrame()
{
    vkWaitForFences(m_vkDevice, 1, &m_vkInFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

    UInt32 lImageIndex;
    VkResult lAcquireResult = vkAcquireNextImageKHR(
        m_vkDevice,
        m_vkSwapchain,
        UINT64_MAX,
        m_vkImageAvailableSemaphores[m_currentFrame], // signal
        VK_NULL_HANDLE,
        &lImageIndex
    );

    if (lAcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        OPAAX_WARNING("[OpaaxVulkanContext] Swapchain out of date!")
        // TODO: recreate swapchain
        return;
    }

    if (lAcquireResult != VK_SUCCESS && lAcquireResult != VK_SUBOPTIMAL_KHR)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to acquire swapchain image!")
        throw std::runtime_error("Failed to acquire swapchain image!");
    }

    // Reset fence for current frame
    vkResetFences(m_vkDevice, 1, &m_vkInFlightFences[m_currentFrame]);

    // Submit command buffer
    VkSubmitInfo lSubmitInfo{};
    lSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_vkImageAvailableSemaphores[m_currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    lSubmitInfo.waitSemaphoreCount = 1;
    lSubmitInfo.pWaitSemaphores = waitSemaphores;
    lSubmitInfo.pWaitDstStageMask = waitStages;

    lSubmitInfo.commandBufferCount = 1;
    lSubmitInfo.pCommandBuffers = &m_vkCommandBuffers[lImageIndex];

    VkSemaphore signalSemaphores[] = { m_vkRenderFinishedSemaphores[m_currentFrame] };
    lSubmitInfo.signalSemaphoreCount = 1;
    lSubmitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_vkGraphicsQueue, 1, &lSubmitInfo, m_vkInFlightFences[m_currentFrame]) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to submit draw command buffer!")
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present
    VkPresentInfoKHR lPresentInfo{};
    lPresentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    lPresentInfo.waitSemaphoreCount = 1;
    lPresentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { m_vkSwapchain };
    lPresentInfo.swapchainCount = 1;
    lPresentInfo.pSwapchains = swapChains;
    lPresentInfo.pImageIndices = &lImageIndex;

    lAcquireResult = vkQueuePresentKHR(m_vkPresentQueue, &lPresentInfo);

    if (lAcquireResult == VK_ERROR_OUT_OF_DATE_KHR || lAcquireResult == VK_SUBOPTIMAL_KHR)
    {
        OPAAX_WARNING("[OpaaxVulkanContext] Swapchain out of date or suboptimal!")
        // TODO: recreate swapchain
    } else if (lAcquireResult != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVulkanContext] Failed to present swapchain image!")
        throw std::runtime_error("Failed to present swapchain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

VkPresentModeKHR OpaaxVulkanContext::ChoosePresentMode(const std::vector<VkPresentModeKHR>& PresentModes)
{
    for (const auto& lMode : PresentModes)
    {
        if (lMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return lMode; // Triple buffering
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D OpaaxVulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& Capabilities)
{
    //TODO Use Custom math
    if (Capabilities.currentExtent.width != UINT32_MAX)
    {
        return Capabilities.currentExtent;
    }
    else
    {
        int lWidth, lHeight;
        glfwGetFramebufferSize(m_window, &lWidth, &lHeight);

        VkExtent2D lActualExtent = {
            static_cast<uint32_t>(lWidth),
            static_cast<uint32_t>(lHeight)
        };

        //TODO Use Custom math
        lActualExtent.width = std::max(Capabilities.minImageExtent.width,
                                       std::min(Capabilities.maxImageExtent.width, lActualExtent.width));
        lActualExtent.height = std::max(Capabilities.minImageExtent.height,
                                        std::min(Capabilities.maxImageExtent.height, lActualExtent.height));

        return lActualExtent;
    }
}

QueueFamilyIndices OpaaxVulkanContext::FindQueueFamiliesForVK(VkPhysicalDevice Device)
{
    QueueFamilyIndices lIndices;

    uint32_t lQueueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> lQueueFamilies(lQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(Device, &lQueueFamilyCount, lQueueFamilies.data());

    int i = 0;
    for (const auto& lQueueFamily : lQueueFamilies)
    {
        if (lQueueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(Device, i, m_vkSurface, &presentSupport);

            if (presentSupport)
            {
                lIndices.PresentFamily = i;
            }

            lIndices.GraphicsFamily = i;
        }

        if (lIndices.IsComplete())
        {
            break;
        }

        i++;
    }

    return lIndices;
}

void OpaaxVulkanContext::Init()
{
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapchain();
    CreateImageViews();
    CreateRenderPass();
    CreateFrameBuffers();
    CreateCommandPool();
    CreateCommandBuffers();
    RecordCommandBuffers();
    CreateSyncObjects();
}

void OpaaxVulkanContext::Shutdown()
{
    if (m_vkCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(m_vkDevice, m_vkImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_vkDevice, m_vkRenderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_vkDevice, m_vkInFlightFences[i], nullptr);
    }

    m_vkImageAvailableSemaphores.clear();
    m_vkRenderFinishedSemaphores.clear();
    m_vkInFlightFences.clear();

    for (auto lFrameBuffer : m_vkSwapchainFrameBuffers)
    {
        vkDestroyFramebuffer(m_vkDevice, lFrameBuffer, nullptr);
    }

    if (m_vkRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_vkDevice, m_vkRenderPass, nullptr);
    }

    for (auto lImageView : m_vkSwapchainImageViews)
    {
        vkDestroyImageView(m_vkDevice, lImageView, nullptr);
    }

    if (m_vkSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_vkDevice, m_vkSwapchain, nullptr);
    }

    if (m_vkDevice != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_vkDevice, nullptr);
    }

    if (gEnableValidationLayers && m_vkDebugMessenger != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessengerEXT(m_vkInstance, m_vkDebugMessenger, nullptr);
    }

    if (m_vkSurface)
    {
        vkDestroySurfaceKHR(m_vkInstance, m_vkSurface, nullptr);
    }

    vkDestroyInstance(m_vkInstance, nullptr);
}
