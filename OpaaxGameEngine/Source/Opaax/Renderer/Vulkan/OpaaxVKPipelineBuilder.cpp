#include "OPpch.h"
#include "Opaax/Renderer/Vulkan/OpaaxVKPipelineBuilder.h"

#include "Opaax/OpaaxStdTypes.h"
#include "Opaax/Log/OPLogMacro.h"

bool OPAAX::RENDERER::VULKAN_HELPER::LoadShaderModule(const char* FilePath, VkDevice Device,
                                                      VkShaderModule* OutShaderModule)
{
    // open the file. With cursor at the end
    std::ifstream lFile(FilePath, std::ios::ate | std::ios::binary);

    if (!lFile.is_open())
    {
        OPAAX_ERROR("[LoadShaderModule] Failed to open file. Path: %1%", %FilePath)
        return false;
    }

    // find what the size of the file is by looking up the location of the cursor
    // because the cursor is at the end, it gives the size directly in bytes
    size_t lFileSize = (size_t)lFile.tellg();

    // spirv expects the buffer to be on uint32, so make sure to reserve a int
    // vector big enough for the entire file
    std::vector<UInt32> lBuffer(lFileSize / sizeof(UInt32));

    // put file cursor at beginning
    lFile.seekg(0);

    // load the entire file into the buffer
    lFile.read(reinterpret_cast<char*>(lBuffer.data()), lFileSize);

    // now that the file is loaded into the buffer, we can close it
    lFile.close();

    // create a new shader module, using the buffer we loaded
    VkShaderModuleCreateInfo lCreateInfo = {};
    lCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    lCreateInfo.pNext = nullptr;

    // codeSize has to be in bytes, so multply the ints in the buffer by size of
    // int to know the real size of the buffer
    lCreateInfo.codeSize = lBuffer.size() * sizeof(uint32_t);
    lCreateInfo.pCode = lBuffer.data();

    // check that the creation goes well.
    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &lCreateInfo, nullptr, &ShaderModule) != VK_SUCCESS)
    {
        OPAAX_ERROR("[LoadShaderModule] Failed to create shader module! Path: %1%", %FilePath)
        return false;
    }

    *OutShaderModule = ShaderModule;

    return true;
}

void OPAAX::RENDERER::VULKAN::OpaaxVKPipelineBuilder::Clear()
{
    // clear all structs we need back to 0 with their correct stype

    InputAssembly = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};

    Rasterizer = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};

    ColorBlendAttachment = {};

    Multisampling = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};

    PipelineLayout = {};

    DepthStencil = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    RenderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};

    ShaderStages.clear();
}

VkPipeline OPAAX::RENDERER::VULKAN::OpaaxVKPipelineBuilder::BuildPipeline(VkDevice Device)
{
    // make viewport state from our stored viewport and scissor.
    // at the moment we won't support multiple viewports or scissors
    VkPipelineViewportStateCreateInfo lViewportState = {};
    lViewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    lViewportState.pNext = nullptr;

    lViewportState.viewportCount = 1;
    lViewportState.scissorCount = 1;

    // setup dummy color blending. We aren't using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.pNext = nullptr;

    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &ColorBlendAttachment;

    // completely clear VertexInputStateCreateInfo, as we have no need for it
    VkPipelineVertexInputStateCreateInfo lVertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
    };

    // build the actual pipeline
    // we now use all the info structs we have been writing into this one
    // to create the pipeline
    VkGraphicsPipelineCreateInfo lPipelineInfo = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    // connect the renderInfo to the pNext extension mechanism
    lPipelineInfo.pNext = &RenderInfo;

    lPipelineInfo.stageCount = static_cast<uint32_t>(ShaderStages.size());
    lPipelineInfo.pStages = ShaderStages.data();
    lPipelineInfo.pVertexInputState = &lVertexInputInfo;
    lPipelineInfo.pInputAssemblyState = &InputAssembly;
    lPipelineInfo.pViewportState = &lViewportState;
    lPipelineInfo.pRasterizationState = &Rasterizer;
    lPipelineInfo.pMultisampleState = &Multisampling;
    lPipelineInfo.pColorBlendState = &colorBlending;
    lPipelineInfo.pDepthStencilState = &DepthStencil;
    lPipelineInfo.layout = PipelineLayout;

    //< build_pipeline_2
    //> build_pipeline_3
    VkDynamicState States[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo lDynamicInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    lDynamicInfo.pDynamicStates = &States[0];
    lDynamicInfo.dynamicStateCount = 2;

    lPipelineInfo.pDynamicState = &lDynamicInfo;
    
    // it's easy to error out on create graphics pipeline, so we handle it a bit
    // better than the common VK_CHECK case
    VkPipeline lNewPipeline;
    if (vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &lPipelineInfo,
                                  nullptr, &lNewPipeline) != VK_SUCCESS)
    {
        OPAAX_ERROR("[OpaaxVKPipelineBuilder] Failed to create pipeline")
        return VK_NULL_HANDLE; // failed to create graphics pipeline
    }
    
    return lNewPipeline;
}

void OPAAX::RENDERER::VULKAN::OpaaxVKPipelineBuilder::SetShaders(VkShaderModule VertexShader,
    VkShaderModule FragmentShader)
{
    ShaderStages.clear();

    ShaderStages.push_back(
        VULKAN_HELPER::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, VertexShader));

    ShaderStages.push_back(
        VULKAN_HELPER::PipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, FragmentShader));
}
