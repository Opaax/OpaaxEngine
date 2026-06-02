#include "VulkanDescriptorLayout.h"

#if OPAAX_HAS_VULKAN

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    VkDescriptorSetLayout BuildSpriteDescriptorSetLayout(VkDevice InDevice)
    {
        VkDescriptorSetLayoutBinding lBindings[2]{};

        // binding 0 : the 16-sampler array (fragment).
        lBindings[0].binding         = 0;
        lBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        lBindings[0].descriptorCount = OPAAX_VULKAN_SPRITE_SAMPLER_COUNT;
        lBindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        // binding 1 : the camera UBO (vertex).
        lBindings[1].binding         = 1;
        lBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        lBindings[1].descriptorCount = 1;
        lBindings[1].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo lInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        lInfo.bindingCount = 2;
        lInfo.pBindings    = lBindings;

        VkDescriptorSetLayout lLayout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(InDevice, &lInfo, nullptr, &lLayout) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("Vulkan: sprite descriptor-set layout creation failed.");
        }
        return lLayout;
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
