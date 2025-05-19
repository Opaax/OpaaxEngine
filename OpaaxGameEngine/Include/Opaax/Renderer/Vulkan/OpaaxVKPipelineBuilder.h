#pragma once
#include "Opaax/OpaaxCoreMacros.h"
#include "OpaaxVulkanInclude.h"

namespace OPAAX
{
    namespace RENDERER
    {
        namespace VULKAN
        {
            class OPAAX_API OpaaxVKPipelineBuilder
            {
            public:
                std::vector<VkPipelineShaderStageCreateInfo> ShaderStages;

                VkPipelineInputAssemblyStateCreateInfo InputAssembly;
                VkPipelineRasterizationStateCreateInfo Rasterizer;
                VkPipelineColorBlendAttachmentState ColorBlendAttachment;
                VkPipelineMultisampleStateCreateInfo Multisampling;
                VkPipelineLayout PipelineLayout;
                VkPipelineDepthStencilStateCreateInfo DepthStencil;
                VkPipelineRenderingCreateInfo RenderInfo;
                VkFormat ColorAttachmentformat;

                OpaaxVKPipelineBuilder() { Clear(); }

                void Clear();

                VkPipeline BuildPipeline(VkDevice Device);
                
                void SetShaders(VkShaderModule VertexShader, VkShaderModule FragmentShader);
//                void SetInputTopology(VkPrimitiveTopology Topology);
//                void SetPolygonMode(VkPolygonMode Mode);
//                void SetCullMode(VkCullModeFlags CullMode, VkFrontFace FrontFace);
//                void SetMultisamplingNone();
//                void DisableBlending();
//                void EnableBlendingAdditive();
//                void EnableBlendingAlphablend();
//
//                void SetColorAttachment_format(VkFormat Format);
//                void SetDepthFormat(VkFormat Format);
//                void DisableDepthTest();
//                void EnableDepthTest(bool DepthWriteEnable, VkCompareOp Op);
            };
        }

        namespace VULKAN_HELPER
        {
            bool LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule* OutShaderModule);

            VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits Stage, VkShaderModule ShaderModule, const char* Entry = "main")
            {
                VkPipelineShaderStageCreateInfo lInfo {};
                lInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                lInfo.pNext = nullptr;

                // shader stage
                lInfo.stage = Stage;
                // module containing the code for this shader stage
                lInfo.module = ShaderModule;
                // the entry point of the shader
                lInfo.pName = Entry;
                
                return lInfo;
            }
        }
    }
}
