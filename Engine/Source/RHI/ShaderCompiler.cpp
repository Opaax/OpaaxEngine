#include "ShaderCompiler.h"

#include "Core/Log/OpaaxLog.h"

// OPAAX_HAS_GLSLANG is defined (0/1) by the engine build; treat absent as 0 for safety.
#ifndef OPAAX_HAS_GLSLANG
#define OPAAX_HAS_GLSLANG 0
#endif

#if OPAAX_HAS_GLSLANG
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <vector>
#endif

namespace Opaax
{
#if OPAAX_HAS_GLSLANG
    namespace
    {
        // glslang requires one process-wide InitializeProcess / FinalizeProcess pair.
        // A function-local static inits on first compile and finalizes at process teardown.
        struct GlslangProcess
        {
            GlslangProcess()  { glslang::InitializeProcess(); }
            ~GlslangProcess() { glslang::FinalizeProcess(); }
        };

        void EnsureGlslangInit()
        {
            static GlslangProcess s_Process;
        }

        EShLanguage ToEShLanguage(EShaderStage InStage)
        {
            switch (InStage)
            {
                case EShaderStage::Vertex:   return EShLangVertex;
                case EShaderStage::Fragment: return EShLangFragment;
            }
            return EShLangVertex;
        }
    }

    TDynArray<Uint32> ShaderCompiler::CompileGLSLToSPIRV(EShaderStage       InStage,
                                                         const OpaaxString& InGlsl,
                                                         const OpaaxString& InDebugName)
    {
        EnsureGlslangInit();

        const EShLanguage lStage = ToEShLanguage(InStage);
        glslang::TShader  lShader(lStage);

        const char* lSrc = InGlsl.CStr();
        lShader.setStrings(&lSrc, 1);

        // Target Vulkan SPIR-V — the one artifact a future Vulkan backend consumes, and that
        // GL_ARB_gl_spirv accepts (requires the GLSL to use explicit bindings/locations).
        constexpr int lGlslVersion = 450;
        lShader.setEnvInput (glslang::EShSourceGlsl, lStage, glslang::EShClientVulkan, lGlslVersion);
        lShader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
        lShader.setEnvTarget(glslang::EShTargetSpv,    glslang::EShTargetSpv_1_0);

        const TBuiltInResource* lResources = GetDefaultResources();
        const EShMessages       lMessages  = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

        if (!lShader.parse(lResources, lGlslVersion, false, lMessages))
        {
            OPAAX_CORE_ERROR("ShaderCompiler: GLSL parse failed for '{}':\n{}\n{}",
                InDebugName, lShader.getInfoLog(), lShader.getInfoDebugLog());
            return {};
        }

        glslang::TProgram lProgram;
        lProgram.addShader(&lShader);
        if (!lProgram.link(lMessages))
        {
            OPAAX_CORE_ERROR("ShaderCompiler: link failed for '{}':\n{}",
                InDebugName, lProgram.getInfoLog());
            return {};
        }

        std::vector<unsigned int> lSpirv;
        glslang::GlslangToSpv(*lProgram.getIntermediate(lStage), lSpirv);

        return TDynArray<Uint32>(lSpirv.begin(), lSpirv.end());
    }
#else  // OPAAX_HAS_GLSLANG

    // glslang not available (no Vulkan SDK at build time). Returns empty SPIR-V — callers
    // fall back to the GLSL source path (OpenGL only; a Vulkan backend needs glslang).
    TDynArray<Uint32> ShaderCompiler::CompileGLSLToSPIRV(EShaderStage /*InStage*/,
                                                         const OpaaxString& /*InGlsl*/,
                                                         const OpaaxString& /*InDebugName*/)
    {
        return {};
    }
#endif // OPAAX_HAS_GLSLANG

} // namespace Opaax
