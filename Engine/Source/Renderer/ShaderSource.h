#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "RHI/Shader.h"   // ShaderDesc

namespace Opaax
{
    // =============================================================================
    // ShaderSource — host-side shader-file loading (file IO stays out of the module).
    //   Splits a single `#type vertex` / `#type fragment` GLSL file into a ShaderDesc and
    //   (when glslang is present) compiles each stage to SPIR-V. The adapter calls this and
    //   passes the resulting ShaderDesc into RenderSystemDesc; the module only ever sees the
    //   already-parsed desc. Shared by the transitional Renderer2D::Init(path) and ShaderAsset.
    // =============================================================================
    namespace ShaderSource
    {
        // Split GLSL sections delimited by a line whose first token is `#type` (`vertex`,
        // `fragment`, `pixel` alias). Content before the first `#type` is ignored.
        OPAAX_API ShaderDesc ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName);

        // Read + parse + (optional) SPIR-V compile. Returns a ShaderDesc with empty stages on
        // failure (missing file / missing a `#type` section) — the caller checks and logs.
        OPAAX_API ShaderDesc LoadShaderDescFromFile(const OpaaxString& InPath);
    }
}
