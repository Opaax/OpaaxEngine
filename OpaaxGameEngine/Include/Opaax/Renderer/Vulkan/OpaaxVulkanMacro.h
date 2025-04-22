#pragma once
#include "Opaax/Log/OPLogMacro.h"

#define VK_CHECK(x)\
do {\
    VkResult lErr = x;\
    if (lErr) {\
        OPAAX_ERROR("Detected Vulkan error: %1%", %string_VkResult(lErr))\
        abort();\
    }\
} while (0)
