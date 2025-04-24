#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKDescriptorLayoutBuilder.h"

#include "Opaax/Renderer/Vulkan/OpaaxVulkanMacro.h"

using namespace OPAAX::RENDERER::VULKAN;

void OpaaxVKDescriptorLayoutBuilder::AddBinding(UInt32 Binding, VkDescriptorType Type)
{
    VkDescriptorSetLayoutBinding lNewbind {};
    lNewbind.binding = Binding;
    lNewbind.descriptorCount = 1;
    lNewbind.descriptorType = Type;

    Bindings.push_back(lNewbind);
}

void OpaaxVKDescriptorLayoutBuilder::Clear()
{
    Bindings.clear();
}

VkDescriptorSetLayout OpaaxVKDescriptorLayoutBuilder::Build(VkDevice Device, VkShaderStageFlags ShaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags Flags)
{
    for (auto& lBind : Bindings)
    {
        lBind.stageFlags |= ShaderStages;
    }

    VkDescriptorSetLayoutCreateInfo lInfo = {.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    lInfo.pNext = pNext;

    lInfo.pBindings = Bindings.data();
    lInfo.bindingCount = static_cast<UInt32>(Bindings.size());
    lInfo.flags = Flags;

    VkDescriptorSetLayout lSet;
    VK_CHECK(vkCreateDescriptorSetLayout(Device, &lInfo, nullptr, &lSet));

    return lSet;
}
