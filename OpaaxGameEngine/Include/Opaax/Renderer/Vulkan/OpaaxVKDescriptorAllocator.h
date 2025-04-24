#pragma once
#include "OpaaxVulkanInclude.h"
#include "Opaax/OpaaxStdTypes.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            struct OPAAX_API OpaaxVKDescriptorAllocator
            {
                struct OpaaxPoolSizeRatio
                {
                    VkDescriptorType Type;
                    float Ratio;
                };

                VkDescriptorPool Pool;

                void InitPool(VkDevice Device, UInt32 MaxSets, std::span<OpaaxPoolSizeRatio> PoolRatios);
                void ClearDescriptors(VkDevice Device);
                void DestroyPool(VkDevice Device);

                VkDescriptorSet Allocate(VkDevice Device, VkDescriptorSetLayout Layout);
            };
        }
    }
}
