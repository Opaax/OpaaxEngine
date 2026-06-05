#pragma once

#include "RHI/Shader.h"

#if OPAAX_HAS_VULKAN

#include <vulkan/vulkan.h>

namespace Opaax
{
    // =============================================================================
    // VulkanShader
    // =============================================================================
    /**
     * @class VulkanShader
     *
     * IShader for Vulkan. Builds one VkShaderModule per stage from the SPIR-V already produced by
     * ShaderCompiler (carried in ShaderDesc) — Vulkan has no GLSL fallback, so empty SPIR-V is a
     * fail-loud error. The modules are consumed by VulkanPipeline at bake time; this object owns
     * them and keeps them alive for the pipeline's lifetime. The name-based Set* setters are
     * no-ops (the engine drives uniforms through the camera UBO, not these).
     */
    class VulkanShader final : public IShader
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit VulkanShader(const ShaderDesc& InDesc);
        ~VulkanShader() override;

        // =============================================================================
        // Get — consumed by VulkanPipeline
        // =============================================================================
    public:
        VkShaderModule GetVertexModule()   const noexcept { return m_VertexModule; }
        VkShaderModule GetFragmentModule() const noexcept { return m_FragmentModule; }
        bool           IsValid()           const noexcept { return m_VertexModule != VK_NULL_HANDLE
                                                                && m_FragmentModule != VK_NULL_HANDLE; }

        // =============================================================================
        // Overrides
        // =============================================================================

        //~Begin IShader interface
    public:
        void Bind()   const override {}
        void Unbind() const override {}

        void SetInt      (const char* /*N*/, Int32           /*V*/)               override {}
        void SetIntArray (const char* /*N*/, const Int32*    /*V*/, Uint32 /*C*/) override {}
        void SetFloat    (const char* /*N*/, float           /*V*/)               override {}
        void SetFloat2   (const char* /*N*/, const Vector2F&  /*V*/)              override {}
        void SetFloat3   (const char* /*N*/, const Vector3F&  /*V*/)              override {}
        void SetFloat4   (const char* /*N*/, const Vector4F&  /*V*/)              override {}
        void SetMat4     (const char* /*N*/, const Matrix44F& /*V*/)              override {}
        //~End IShader interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        VkDevice       m_Device         = VK_NULL_HANDLE;   // borrowed
        VkShaderModule m_VertexModule   = VK_NULL_HANDLE;
        VkShaderModule m_FragmentModule = VK_NULL_HANDLE;
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
