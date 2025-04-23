#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKDescriptorAllocator.h"

#include "Opaax/Renderer/Vulkan/OpaaxVulkanMacro.h"

using namespace OPAAX::RENDERER::VULKAN;

void OpaaxVKDescriptorAllocator::InitPool(VkDevice Device, UInt32 MaxSets, std::span<OpaaxPoolSizeRatio> PoolRatios)
{
    std::vector<VkDescriptorPoolSize> lPoolSizes;
    for (OpaaxPoolSizeRatio lRatio : PoolRatios)
    {
        lPoolSizes.push_back(VkDescriptorPoolSize{
            .type = lRatio.Type,
            .descriptorCount = static_cast<UInt32>(lRatio.Ratio * MaxSets)
        });
    }

    VkDescriptorPoolCreateInfo lPoolInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    lPoolInfo.flags = 0;
    lPoolInfo.maxSets = MaxSets;
    lPoolInfo.poolSizeCount = static_cast<UInt32>(lPoolSizes.size());
    lPoolInfo.pPoolSizes = lPoolSizes.data();

    vkCreateDescriptorPool(Device, &lPoolInfo, nullptr, &Pool);
}

void OpaaxVKDescriptorAllocator::ClearDescriptors(VkDevice Device)
{
    vkResetDescriptorPool(Device, Pool, 0);
}

void OpaaxVKDescriptorAllocator::DestroyPool(VkDevice Device)
{
    vkDestroyDescriptorPool(Device,Pool,nullptr);
}

VkDescriptorSet OpaaxVKDescriptorAllocator::Allocate(VkDevice Device, VkDescriptorSetLayout Layout)
{
    VkDescriptorSetAllocateInfo lAllocInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    lAllocInfo.pNext = nullptr;
    lAllocInfo.descriptorPool = Pool;
    lAllocInfo.descriptorSetCount = 1;
    lAllocInfo.pSetLayouts = &Layout;

    VkDescriptorSet lDescSet;
    VK_CHECK(vkAllocateDescriptorSets(Device, &lAllocInfo, &lDescSet));

    return lDescSet;
}
