#include "Renderer/Vulkan/OpaaxVulkanGraphicsPipeline.h"
#include "Renderer/OpaaxShaderHelper.h"
#include "Renderer/Vulkan/OpaaxVulkanShaderHelper.h"

using namespace OPAAX::VULKAN;

OpaaxVulkanGraphicsPipeline::OpaaxVulkanGraphicsPipeline(VkDevice LogicalDevice, VkRenderPass RenderPass)
{
    CreateGraphicsPipeline(LogicalDevice, RenderPass);
}

OpaaxVulkanGraphicsPipeline::~OpaaxVulkanGraphicsPipeline() {}

void OpaaxVulkanGraphicsPipeline::Cleanup(VkDevice LogicalDevice)
{
    vkDestroyPipeline(LogicalDevice, m_vkGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(LogicalDevice, m_vkPipelineLayout, nullptr);
}

void OpaaxVulkanGraphicsPipeline::CreateGraphicsPipeline(VkDevice LogicalDevice, VkRenderPass RenderPass)
{
    auto lVertShaderCode = SHADER_HELPER::ReadFile("Shaders/SimpleShader.vert.spv");
    auto lFragShaderCode = SHADER_HELPER::ReadFile("Shaders/SimpleShader.frag.spv");

    VkShaderModule lVertShaderModule = VULKAN_SHADER_HELPER::CreateShaderModule(lVertShaderCode, LogicalDevice);
    VkShaderModule lFragShaderModule = VULKAN_SHADER_HELPER::CreateShaderModule(lFragShaderCode, LogicalDevice);

    VkPipelineShaderStageCreateInfo lVertShaderStageInfo{};
    lVertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lVertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    lVertShaderStageInfo.module = lVertShaderModule;
    lVertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo lFragShaderStageInfo{};
    lFragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    lFragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    lFragShaderStageInfo.module = lFragShaderModule;
    lFragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo lShaderStages[] = {lVertShaderStageInfo, lFragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo lVertexInputInfo{};
    lVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    lVertexInputInfo.vertexBindingDescriptionCount = 0;
    lVertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo lInputAssembly{};
    lInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    lInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    lInputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo lViewportState{};
    lViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    lViewportState.viewportCount = 1;
    lViewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo lRasterizer{};
    lRasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    lRasterizer.depthClampEnable = VK_FALSE;
    lRasterizer.rasterizerDiscardEnable = VK_FALSE;
    lRasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    lRasterizer.lineWidth = 1.0f;
    lRasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    lRasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    lRasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo lMultisampling{};
    lMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    lMultisampling.sampleShadingEnable = VK_FALSE;
    lMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState lColorBlendAttachment{};
    lColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    lColorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo lColorBlending{};
    lColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    lColorBlending.logicOpEnable = VK_FALSE;
    lColorBlending.logicOp = VK_LOGIC_OP_COPY;
    lColorBlending.attachmentCount = 1;
    lColorBlending.pAttachments = &lColorBlendAttachment;
    lColorBlending.blendConstants[0] = 0.0f;
    lColorBlending.blendConstants[1] = 0.0f;
    lColorBlending.blendConstants[2] = 0.0f;
    lColorBlending.blendConstants[3] = 0.0f;

    std::vector<VkDynamicState> lDynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo lDynamicState{};
    lDynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    lDynamicState.dynamicStateCount = static_cast<uint32_t>(lDynamicStates.size());
    lDynamicState.pDynamicStates = lDynamicStates.data();

    VkPipelineLayoutCreateInfo lPipelineLayoutInfo{};
    lPipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lPipelineLayoutInfo.setLayoutCount = 0;
    lPipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(LogicalDevice, &lPipelineLayoutInfo, nullptr, &m_vkPipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo lPipelineInfo{};
    lPipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    lPipelineInfo.stageCount = 2;
    lPipelineInfo.pStages = lShaderStages;
    lPipelineInfo.pVertexInputState = &lVertexInputInfo;
    lPipelineInfo.pInputAssemblyState = &lInputAssembly;
    lPipelineInfo.pViewportState = &lViewportState;
    lPipelineInfo.pRasterizationState = &lRasterizer;
    lPipelineInfo.pMultisampleState = &lMultisampling;
    lPipelineInfo.pColorBlendState = &lColorBlending;
    lPipelineInfo.pDynamicState = &lDynamicState;
    lPipelineInfo.layout = m_vkPipelineLayout;
    lPipelineInfo.renderPass = RenderPass;
    lPipelineInfo.subpass = 0;
    lPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(LogicalDevice, VK_NULL_HANDLE, 1, &lPipelineInfo, nullptr, &m_vkGraphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(LogicalDevice, lFragShaderModule, nullptr);
    vkDestroyShaderModule(LogicalDevice, lVertShaderModule, nullptr);
}
