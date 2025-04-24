#pragma once
#include "OpaaxVKTypes.h"
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxCoreMacros.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxVKDescriptorLayoutBuilder
            {
                VecDescSetLayoutBinding Bindings;

                void AddBinding(UInt32 Binding, VkDescriptorType Type);
                void Clear();
                
                VkDescriptorSetLayout Build(VkDevice Device, VkShaderStageFlags ShaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags Flags = 0);
            };
        }
    }
}
