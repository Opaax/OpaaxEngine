#include "VulkanPipeline.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanDescriptorLayout.h"
#include "RHI/Shader.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // NOTE: the IPipeline::Create factory dispatch lives in RHI/BackendFactory.cpp.

    namespace
    {
        VkFormat ToVkFormat(EShaderDataType InType)
        {
            switch (InType)
            {
                case EShaderDataType::Float:  return VK_FORMAT_R32_SFLOAT;
                case EShaderDataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
                case EShaderDataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
                case EShaderDataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
                case EShaderDataType::Int:    return VK_FORMAT_R32_SINT;
                case EShaderDataType::Int2:   return VK_FORMAT_R32G32_SINT;
                case EShaderDataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
                case EShaderDataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
                default:                      return VK_FORMAT_UNDEFINED;
            }
        }
    }

    VulkanPipeline::VulkanPipeline(const PipelineDesc& InDesc)
    {
        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)
        m_Device = lDevice->GetDevice();

        auto* lShader = static_cast<VulkanShader*>(InDesc.Shader);
        if (!lShader || !lShader->IsValid())
        {
            OPAAX_CORE_ERROR("VulkanPipeline: invalid shader — cannot bake pipeline '{}'.", InDesc.DebugName);
            return;
        }

        // ---- Shader stages ----
        VkPipelineShaderStageCreateInfo lStages[2]{};
        lStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        lStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        lStages[0].module = lShader->GetVertexModule();
        lStages[0].pName  = "main";
        lStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        lStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        lStages[1].module = lShader->GetFragmentModule();
        lStages[1].pName  = "main";

        // ---- Vertex input (one interleaved binding, attributes from the layout) ----
        VkVertexInputBindingDescription lBinding{};
        lBinding.binding   = 0;
        lBinding.stride    = InDesc.VertexLayout.GetStride();
        lBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        TDynArray<VkVertexInputAttributeDescription> lAttribs;
        Uint32 lLocation = 0;
        for (const BufferElement& lElem : InDesc.VertexLayout.GetElements())
        {
            VkVertexInputAttributeDescription lAttr{};
            lAttr.location = lLocation++;
            lAttr.binding  = 0;
            lAttr.format   = ToVkFormat(lElem.Type);
            lAttr.offset   = lElem.Offset;
            lAttribs.push_back(lAttr);
        }

        VkPipelineVertexInputStateCreateInfo lVertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
        lVertexInput.vertexBindingDescriptionCount   = 1;
        lVertexInput.pVertexBindingDescriptions      = &lBinding;
        lVertexInput.vertexAttributeDescriptionCount = static_cast<Uint32>(lAttribs.size());
        lVertexInput.pVertexAttributeDescriptions    = lAttribs.data();

        // ---- Input assembly ----
        VkPipelineInputAssemblyStateCreateInfo lInputAsm{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        lInputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // ---- Viewport + scissor are dynamic (set per pass from the command buffer) ----
        VkPipelineViewportStateCreateInfo lViewport{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        lViewport.viewportCount = 1;
        lViewport.scissorCount  = 1;

        const VkDynamicState lDynamics[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo lDynamic{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        lDynamic.dynamicStateCount = 2;
        lDynamic.pDynamicStates    = lDynamics;

        // ---- Rasterizer (2D: fill, no cull) ----
        VkPipelineRasterizationStateCreateInfo lRaster{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        lRaster.polygonMode = VK_POLYGON_MODE_FILL;
        lRaster.cullMode    = VK_CULL_MODE_NONE;
        lRaster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        lRaster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo lMultisample{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        lMultisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // ---- Blend (standard 2D alpha) ----
        VkPipelineColorBlendAttachmentState lBlendAttach{};
        lBlendAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                    | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        if (InDesc.Blend == EBlendMode::Alpha)
        {
            lBlendAttach.blendEnable         = VK_TRUE;
            lBlendAttach.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            lBlendAttach.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            lBlendAttach.colorBlendOp        = VK_BLEND_OP_ADD;
            lBlendAttach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            lBlendAttach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            lBlendAttach.alphaBlendOp        = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo lBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        lBlend.attachmentCount = 1;
        lBlend.pAttachments    = &lBlendAttach;

        // ---- Layout (descriptor set 0 = the sprite layout) ----
        m_SetLayout = BuildSpriteDescriptorSetLayout(m_Device);

        VkPipelineLayoutCreateInfo lLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        lLayoutInfo.setLayoutCount = 1;
        lLayoutInfo.pSetLayouts    = &m_SetLayout;
        if (vkCreatePipelineLayout(m_Device, &lLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanPipeline: pipeline layout creation failed.");
            return;
        }

        // ---- Dynamic rendering: declare the color attachment format (no VkRenderPass) ----
        const VkFormat lColorFormat = VulkanFrameContext::ColorFormat();
        VkPipelineRenderingCreateInfo lRendering{ VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
        lRendering.colorAttachmentCount    = 1;
        lRendering.pColorAttachmentFormats = &lColorFormat;

        // ---- Bake ----
        VkGraphicsPipelineCreateInfo lInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        lInfo.pNext               = &lRendering;
        lInfo.stageCount          = 2;
        lInfo.pStages             = lStages;
        lInfo.pVertexInputState   = &lVertexInput;
        lInfo.pInputAssemblyState = &lInputAsm;
        lInfo.pViewportState      = &lViewport;
        lInfo.pRasterizationState = &lRaster;
        lInfo.pMultisampleState   = &lMultisample;
        lInfo.pColorBlendState    = &lBlend;
        lInfo.pDynamicState       = &lDynamic;
        lInfo.layout              = m_PipelineLayout;

        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &lInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
        {
            OPAAX_CORE_ERROR("VulkanPipeline: vkCreateGraphicsPipelines failed for '{}'.", InDesc.DebugName);
            return;
        }

        OPAAX_CORE_INFO("VulkanPipeline: '{}' baked.", InDesc.DebugName);
    }

    VulkanPipeline::~VulkanPipeline()
    {
        if (m_Pipeline)       { vkDestroyPipeline(m_Device, m_Pipeline, nullptr); }
        if (m_PipelineLayout) { vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr); }
        if (m_SetLayout)      { vkDestroyDescriptorSetLayout(m_Device, m_SetLayout, nullptr); }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
