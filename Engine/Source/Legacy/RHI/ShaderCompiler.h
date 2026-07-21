#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    // =============================================================================
    // EShaderStage
    // =============================================================================
    enum class EShaderStage
    {
        Vertex,
        Fragment
    };

    // =============================================================================
    // ShaderCompiler
    // =============================================================================
    /**
     * @class ShaderCompiler
     *
     * Compiles Vulkan-flavored GLSL to SPIR-V via glslang. SPIR-V is the single portable
     * shader IR: OpenGL consumes it through GL_ARB_gl_spirv, Vulkan natively. Backend-
     * neutral — glslang types never leak past this TU.
     */
    class OPAAX_API ShaderCompiler
    {
    public:
        /**
         * Compile one GLSL stage to SPIR-V.
         * @param InStage     vertex or fragment
         * @param InGlsl      stage GLSL source (Vulkan rules: explicit bindings/locations)
         * @param InDebugName label used in compile-error logs
         * @return SPIR-V words, or an empty array on failure (logged fail-loud).
         */
        static TDynArray<Uint32> CompileGLSLToSPIRV(EShaderStage       InStage,
                                                    const OpaaxString& InGlsl,
                                                    const OpaaxString& InDebugName);
    };

} // namespace Opaax
