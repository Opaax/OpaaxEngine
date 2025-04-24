#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/Renderer/OpaaxShaderTypes.h"
#include "Opaax/OpaaxCoreMacros.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxVKComputeEffect
            {
                const char* Name;

                VkPipeline Pipeline;
                VkPipelineLayout Layout;

                SHADER::OpaaxComputeShaderPushConstants Data;
            };
        }
    }
}

