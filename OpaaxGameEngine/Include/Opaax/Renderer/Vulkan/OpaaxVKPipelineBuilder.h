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

                /**
                 * @brief Sets the vertex and fragment shaders for the Vulkan pipeline.
                 *
                 * This method configures the shader stages of the pipeline with the specified
                 * vertex and fragment shader modules. The shaders are used during the pipeline
                 * creation and are critical in defining the programmable stages of the pipeline.
                 *
                 * @param VertexShader The Vulkan shader module representing the vertex shader.
                 *                     This shader module will define the vertex processing stage.
                 * @param FragmentShader The Vulkan shader module representing the fragment shader.
                 *                       This shader module will define the fragment processing stage.
                 */
                void SetShaders(VkShaderModule VertexShader, VkShaderModule FragmentShader);

                /**
                 * @brief Configures the input topology for the Vulkan pipeline.
                 *
                 * This method sets the topology used in the input assembly stage of the pipeline,
                 * determining how the vertices of a primitive are assembled.
                 *
                 * @param Topology The Vulkan primitive topology specifying the type of primitive
                 *                 to be constructed, such as triangle list, line list, or point list.
                 */
                void SetInputTopology(VkPrimitiveTopology Topology);

                /**
                 * @brief Sets the polygon mode for the Vulkan pipeline rasterizer.
                 *
                 * This method configures the polygon rendering mode for the rasterizer stage
                 * of the Vulkan pipeline. The polygon mode determines how polygons are
                 * rasterized, such as filled, outlined, or point-based rendering.
                 *
                 * @param Mode The Vulkan polygon mode specifying the rasterization mode,
                 *             such as VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE, or
                 *             VK_POLYGON_MODE_POINT. It affects how the interior of polygons
                 *             are processed.
                 */
                void SetPolygonMode(VkPolygonMode Mode);

                /**
                 * @brief Configures the culling mode and front-facing polygon orientation for the Vulkan pipeline rasterizer.
                 *
                 * This method sets the cull mode and front face winding order for the rasterization stage
                 * of the Vulkan pipeline. The cull mode determines which polygons are culled (back-facing,
                 * front-facing, or none), and the front face defines the vertex winding order that is
                 * considered front-facing.
                 *
                 * @param CullMode Specifies the culling mode for the rasterization stage. It can take values like
                 *                 VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT, or
                 *                 VK_CULL_MODE_FRONT_AND_BACK.
                 * @param FrontFace Specifies the front face orientation using winding order. Common values include
                 *                  VK_FRONT_FACE_CLOCKWISE and VK_FRONT_FACE_COUNTER_CLOCKWISE.
                 */
                void SetCullMode(VkCullModeFlags CullMode, VkFrontFace FrontFace);

                /**
                 * @brief Configures the Vulkan pipeline to disable multisampling.
                 *
                 * This method sets the pipeline's multisampling state to use a single sample per pixel.
                 * It disables sample shading, alpha-to-coverage, and other features associated with multisampling.
                 * This configuration is typically used when multisampling is not required, providing a basic
                 * rendering configuration with no additional sampling overhead.
                 */
                void SetMultisamplingNone();

                /**
                 * @brief Disables blending for the Vulkan pipeline.
                 *
                 * This method configures the pipeline to disable color blending. It sets the
                 * color write mask to include all color components (R, G, B, and A) while
                 * ensuring that blending is turned off for the associated render target.
                 */
                void DisableBlending();
                
                //void EnableBlendingAdditive();
                //void EnableBlendingAlphablend();

                /**
                 * @brief Sets the color attachment format for the Vulkan pipeline.
                 *
                 * This method specifies the format of the color attachment used by the Vulkan pipeline.
                 * The format determines how color data is stored in the framebuffer and is essential
                 * for proper rendering and compatibility with the configured render pass.
                 *
                 * @param Format The Vulkan format used for the color attachment.
                 *               It defines the layout and encoding of pixel data in the framebuffer.
                 */
                void SetColorAttachmentFormat(VkFormat Format);
                void SetDepthFormat(VkFormat Format);
                void DisableDepthTest();
                //void EnableDepthTest(bool DepthWriteEnable, VkCompareOp Op);
            };
        }

        namespace VULKAN_HELPER
        {
            bool LoadShaderModule(const char* FilePath, VkDevice Device, VkShaderModule* OutShaderModule);
            VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo();
            VkPipelineShaderStageCreateInfo PipelineShaderStageCreateInfo(VkShaderStageFlagBits Stage, VkShaderModule ShaderModule, const char* Entry = "main");
        }
    }
}
