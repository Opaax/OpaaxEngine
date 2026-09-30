#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "RHI/Shader.h"   // ShaderDesc

namespace Opaax
{
    // =============================================================================
    // ShaderSource — shader source text to ShaderDesc. No file IO (the host reads the file).
    //   Splits a `#type vertex` / `#type fragment` GLSL source, and compiles each stage to SPIR-V
    //   when glslang is available.
    // =============================================================================
    namespace ShaderSource
    {
        // Splits GLSL sections that start with a `#type` line (vertex, fragment, pixel).
        // Text before the first `#type` is ignored.
        OPAAX_API ShaderDesc ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName);

        // Parses and compiles (SPIR-V, optional). Returns empty stages when there is no `#type`
        // section (e.g. an empty string for a missing file); the caller checks and logs.
        OPAAX_API ShaderDesc FromSource(const OpaaxString& InSource, const OpaaxString& InDebugName);
    }
}
