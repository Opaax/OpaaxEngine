#pragma once

#include "Core/EngineAPI.h"
#include "Core/String/OpaaxString.hpp"
#include "RHI/Shader.h"   // ShaderDesc

namespace Opaax
{
    // =============================================================================
    // ShaderSource — shader SOURCE TEXT to ShaderDesc. File IO genuinely stays out of the module:
    //   nothing here opens a file, and this header includes no <fstream>. The HOST reads the text
    //   (RendererManager, the adapter that already resolves IPaths) and passes it in; the portable
    //   Renderer only ever sees a string it was handed.
    //
    //   That was a lie until 2026-07-28 — this header claimed it while declaring
    //   LoadShaderDescFromFile right below, which opened the file itself.
    //
    //   Splits a single `#type vertex` / `#type fragment` GLSL source into a ShaderDesc and (when
    //   glslang is present) compiles each stage to SPIR-V.
    // =============================================================================
    namespace ShaderSource
    {
        // Split GLSL sections delimited by a line whose first token is `#type` (`vertex`,
        // `fragment`, `pixel` alias). Content before the first `#type` is ignored. Pure.
        OPAAX_API ShaderDesc ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName);

        // Parse + (optional) SPIR-V compile — the whole source-to-desc step, both of which are
        // Renderer work. Returns a ShaderDesc with EMPTY STAGES when InSource has no `#type`
        // section (including when the host handed over an empty string because the file was
        // missing); the caller checks and logs.
        OPAAX_API ShaderDesc FromSource(const OpaaxString& InSource, const OpaaxString& InDebugName);
    }
}
