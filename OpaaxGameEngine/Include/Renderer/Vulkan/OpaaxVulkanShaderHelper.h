#pragma once
#include <stdexcept>
#include <vector>

#include "Core/OPLogMacro.h"
#include "vulkan/vulkan_core.h"

namespace OPAAX
{
    namespace VULKAN_SHADER_HELPER
    {
        static VkShaderModule CreateShaderModule(const std::vector<char>& Code, VkDevice Device)
        {
            VkShaderModuleCreateInfo lCreateInfo{};
            lCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            lCreateInfo.codeSize = Code.size();
            lCreateInfo.pCode = reinterpret_cast<const UInt32*>(Code.data());

            VkShaderModule lShaderModule;
            if (vkCreateShaderModule(Device, &lCreateInfo, nullptr, &lShaderModule) != VK_SUCCESS)
            {
                OPAAX_ERROR("[VULKAN_SHADER_HELPER][VkShaderModule] Failed to create shader module!")
                throw std::runtime_error("Failed to create shader module!");
            }

            return lShaderModule;
        }
    }
}
