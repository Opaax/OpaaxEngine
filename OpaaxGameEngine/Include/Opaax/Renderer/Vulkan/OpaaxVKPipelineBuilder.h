#pragma once
#include "Opaax/OpaaxCoreMacros.h"
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            class OPAAX_API OpaaxVKPipelineBuilder
            {
            public:
    
            };
        }

        namespace VULKAN_HELPER
        {
            bool LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule* OutShaderModule);
        }
    }
}
