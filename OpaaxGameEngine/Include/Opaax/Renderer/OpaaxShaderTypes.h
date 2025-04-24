#pragma once
#include <glm/vec4.hpp>

namespace OPAAX
{
    namespace SHADER
    {
        struct OPAAX_API OpaaxComputeShaderPushConstants
        {
            glm::vec4 data1;
            glm::vec4 data2;
            glm::vec4 data3;
            glm::vec4 data4;
        };
    }
}
