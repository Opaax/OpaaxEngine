#pragma once
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN_CONST
        {
            const std::vector<const char*> G_VALIDATION_LAYERS =
            {
                "VK_LAYER_KHRONOS_validation"
            };

#ifdef NDEBUG
            constexpr bool G_ENABLE_VALIDATION_LAYERS = false;
#else
            constexpr bool G_ENABLE_VALIDATION_LAYERS = true;
#endif

            const std::vector<const char*> G_DEVICES_EXTENSIONS =
            {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME
            };

            constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        }
    }
}
