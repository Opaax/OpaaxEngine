#pragma once

#include "RHI/Shader.h"

#if OPAAX_HAS_VULKAN

namespace Opaax
{
    // =============================================================================
    // VulkanShader  (Phase 2 stub)
    // =============================================================================
    // No VkShaderModule yet — the SPIR-V in ShaderDesc is consumed in Phase 3 (VulkanPipeline).
    // The name-based setters are no-ops (the engine drives uniforms via the UBO, not these).
    class VulkanShader final : public IShader
    {
    public:
        explicit VulkanShader(const ShaderDesc& /*InDesc*/) {}

        void Bind()   const override {}
        void Unbind() const override {}

        void SetInt      (const char* /*N*/, Int32           /*V*/)                  override {}
        void SetIntArray (const char* /*N*/, const Int32*    /*V*/, Uint32 /*C*/)    override {}
        void SetFloat    (const char* /*N*/, float           /*V*/)                  override {}
        void SetFloat2   (const char* /*N*/, const Vector2F&  /*V*/)                 override {}
        void SetFloat3   (const char* /*N*/, const Vector3F&  /*V*/)                 override {}
        void SetFloat4   (const char* /*N*/, const Vector4F&  /*V*/)                 override {}
        void SetMat4     (const char* /*N*/, const Matrix44F& /*V*/)                 override {}
    };

} // namespace Opaax

#endif // OPAAX_HAS_VULKAN
