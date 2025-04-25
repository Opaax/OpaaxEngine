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
            /**
             * @struct OpaaxVKComputeEffect
             * @brief Struct representing a Vulkan compute effect in the Opaax engine.
             *
             * This struct encapsulates the essential components required for executing a compute pipeline in Vulkan within the Opaax engine.
             * It includes the name of the effect, the Vulkan pipeline, associated layout, and the push constant data for compute shaders.
             * It is utilized for creating and managing individual compute effects, such as gradient or sky shaders, in Vulkan-based rendering pipelines.
             *
             * Attributes:
             * - `Name`     : A constant character pointer representing the name of the compute effect.
             * - `Pipeline` : A Vulkan handle to the compute pipeline associated with this effect.
             * - `Layout`   : A Vulkan handle to the pipeline layout used by this effect.
             * - `Data`     : Push constant data required for the compute shader, encapsulated in the OpaaxComputeShaderPushConstants structure.
             */
            struct OPAAX_API OpaaxVKComputeEffect
            {
                const char*         Name;
                VkPipeline          Pipeline;
                VkPipelineLayout    Layout;

                SHADER::OpaaxComputeShaderPushConstants Data;
            };
        }
    }
}

