#include "VulkanShader.h"

#if OPAAX_HAS_VULKAN

#include "VulkanFrameContext.h"
#include "VulkanDevice.h"
#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // NOTE: the IShader::Create factory dispatch lives in RHI/BackendFactory.cpp.

    namespace
    {
        VkShaderModule MakeModule(VkDevice InDevice, const TDynArray<Uint32>& InSpirv, const char* InStage)
        {
            if (InSpirv.empty())
            {
                OPAAX_CORE_ERROR("VulkanShader: empty {} SPIR-V — the Vulkan backend requires glslang "
                                 "(no GLSL fallback). Build with the Vulkan SDK present.", InStage);
                return VK_NULL_HANDLE;
            }

            VkShaderModuleCreateInfo lInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
            lInfo.codeSize = InSpirv.size() * sizeof(Uint32);   // bytes
            lInfo.pCode    = InSpirv.data();

            VkShaderModule lModule = VK_NULL_HANDLE;
            if (vkCreateShaderModule(InDevice, &lInfo, nullptr, &lModule) != VK_SUCCESS)
            {
                OPAAX_CORE_ERROR("VulkanShader: vkCreateShaderModule failed for {} stage.", InStage);
                return VK_NULL_HANDLE;
            }
            return lModule;
        }
    }

    VulkanShader::VulkanShader(const ShaderDesc& InDesc)
    {
        VulkanDevice* lDevice = VulkanFrameContext::Device();
        OPAAX_CORE_ASSERT(lDevice)
        m_Device = lDevice->GetDevice();

        m_VertexModule   = MakeModule(m_Device, InDesc.VertexSpirv,   "vertex");
        m_FragmentModule = MakeModule(m_Device, InDesc.FragmentSpirv, "fragment");

        if (IsValid())
        {
            OPAAX_CORE_INFO("VulkanShader: '{}' modules created.", InDesc.DebugName.CStr());
        }
    }

    VulkanShader::~VulkanShader()
    {
        if (m_VertexModule)   { vkDestroyShaderModule(m_Device, m_VertexModule, nullptr); }
        if (m_FragmentModule) { vkDestroyShaderModule(m_Device, m_FragmentModule, nullptr); }
    }

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
