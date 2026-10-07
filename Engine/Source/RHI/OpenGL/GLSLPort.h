#pragma once

#include <string>

#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // GLSLPort — shaders are written once, in GLSL 4.50 with explicit `binding = N` (what a
    //   Vulkan path needs). OpenGL runs them as GLSL 4.10 core, the most macOS offers: the version
    //   line is rewritten and each binding qualifier is taken out, to be applied once the program
    //   is linked (glUniformBlockBinding, sampler uniforms).
    //   Pure text processing: no GL call.
    // =============================================================================
    namespace GLSLPort
    {
        /** One `binding = N` taken out of a uniform declaration. */
        struct ResourceBinding
        {
            std::string Name;            // the uniform block's name, or the sampler's
            Int32       Binding   = 0;   // the unit (sampler) or binding point (block)
            Int32       ArraySize = 1;   // a sampler array covers Binding .. Binding + ArraySize - 1
            bool        bBlock    = false;
        };

        /**
         * InSource as GLSL 4.10 core.
         * @param OutBindings Cleared, then filled with the bindings taken out, in source order
         */
        std::string To410(const std::string& InSource, TDynArray<ResourceBinding>& OutBindings);
    }
}
