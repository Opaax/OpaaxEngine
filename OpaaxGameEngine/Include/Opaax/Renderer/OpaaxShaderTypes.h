#pragma once
#include <glm/vec4.hpp>

namespace OPAAX
{
    namespace SHADER
    {
        /**
         * @struct OpaaxComputeShaderPushConstants
         *
         * Represents a structure containing push constant data for a compute shader in
         * the Opaax engine. Push constants are a mechanism to efficiently pass a small
         * amount of data directly to a shader pipeline without the need for a larger uniform
         * or descriptor buffer.
         *
         * This structure contains four `glm::vec4` fields which are used to define the data
         * being passed to the compute shader. Each `vec4` can represent various attributes,
         * such as colors, configuration parameters, or other small sets of per-execution
         * settings.
         *
         * Typical usage of this structure is within Vulkan pipelines where it is bound as a
         * push constant for compute stages.
         *
         * @note This structure is primarily utilized in Vulkan shader pipelines and relies
         * on compatible shader implementations using appropriate bindings for these constants.
         */
        struct OPAAX_API OpaaxComputeShaderPushConstants
        {
            glm::vec4 Data1;
            glm::vec4 Data2;
            glm::vec4 Data3;
            glm::vec4 Data4;
        };
    }
}
