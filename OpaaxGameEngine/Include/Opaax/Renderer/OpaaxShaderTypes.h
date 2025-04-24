#pragma once
#include <glm/vec4.hpp>

namespace OPAAX
{
    namespace SHADER
    {
        struct OPAAX_API OpaaxComputeShaderPushConstants
        {
            glm::vec4 Data1;
            glm::vec4 Data2;
            glm::vec4 Data3;
            glm::vec4 Data4;
        };
    }
}
