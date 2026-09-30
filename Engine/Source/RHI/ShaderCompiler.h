#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"

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
     * Compiles GLSL to SPIR-V with glslang (used by OpenGL through GL_ARB_gl_spirv and by Vulkan).
     */
    class OPAAX_API ShaderCompiler
    {
    public:
        /**
         * Compiles one GLSL stage to SPIR-V.
         * @param InStage     Vertex or fragment
         * @param InGlsl      Stage source (explicit bindings/locations)
         * @param InDebugName Name used in error logs
         * @return SPIR-V words, or empty on failure (logged)
         */
        static TDynArray<Uint32> CompileGLSLToSPIRV(EShaderStage       InStage,
                                                    const OpaaxString& InGlsl,
                                                    const OpaaxString& InDebugName);
    };

} // namespace Opaax
