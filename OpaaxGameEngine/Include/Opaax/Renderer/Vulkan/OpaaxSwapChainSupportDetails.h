#pragma once
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OpaaxSwapChainSupportDetails
            {
                VkSurfaceCapabilitiesKHR        Capabilities;
                std::vector<VkSurfaceFormatKHR> Formats;
                std::vector<VkPresentModeKHR>   PresentModes;
            };
        }
    }
}
