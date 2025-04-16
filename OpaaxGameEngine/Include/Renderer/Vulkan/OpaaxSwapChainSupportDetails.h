#pragma once
#include <vector>
#include <vulkan/vulkan_core.h>

namespace OPAAX
{
    namespace VULKAN
    {
        struct OpaaxSwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR Capabilities;
            std::vector<VkSurfaceFormatKHR> Formats;
            std::vector<VkPresentModeKHR> PresentModes;
        };
    }
}
