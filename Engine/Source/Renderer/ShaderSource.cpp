#include "ShaderSource.h"

#include "RHI/ShaderCompiler.h"
#include "Application/Services/ILogger.h"

#include <sstream>
#include <string>

namespace Opaax
{
    inline constexpr LogCategory LogShaderSource{"ShaderSource"};

    ShaderDesc ShaderSource::ParseShaderStages(const OpaaxString& InSource, const OpaaxString& InDebugName)
    {
        ShaderDesc lDesc;
        lDesc.DebugName = InDebugName;

        std::istringstream lStream(InSource.CStr());
        std::string        lLine;
        std::string        lVert;
        std::string        lFrag;
        int                lStage = -1; // 0 = vertex, 1 = fragment, -1 = none/unknown

        while (std::getline(lStream, lLine))
        {
            if (!lLine.empty() && lLine.back() == '\r') { lLine.pop_back(); } // CRLF tolerance

            const size_t lFirst = lLine.find_first_not_of(" \t");
            if (lFirst != std::string::npos && lLine.compare(lFirst, 5, "#type") == 0)
            {
                const std::string lRest = lLine.substr(lFirst + 5);
                if      (lRest.find("vertex")   != std::string::npos) { lStage = 0; }
                else if (lRest.find("fragment") != std::string::npos ||
                         lRest.find("pixel")    != std::string::npos) { lStage = 1; }
                else                                                  { lStage = -1; }
                continue;
            }

            if      (lStage == 0) { lVert += lLine; lVert += '\n'; }
            else if (lStage == 1) { lFrag += lLine; lFrag += '\n'; }
        }

        lDesc.VertexSrc   = OpaaxString(lVert.c_str());
        lDesc.FragmentSrc = OpaaxString(lFrag.c_str());
        return lDesc;
    }

    ShaderDesc ShaderSource::FromSource(const OpaaxString& InSource, const OpaaxString& InDebugName)
    {
        ShaderDesc lDesc = ParseShaderStages(InSource, InDebugName);
        if (lDesc.VertexSrc.IsEmpty() || lDesc.FragmentSrc.IsEmpty())
        {
            OPAAX_LOG(LogShaderSource, Error, "'{}' missing a vertex or fragment '#type' section", InDebugName.CStr());
            return ShaderDesc{};
        }

        // Compile both stages to SPIR-V when glslang is available (GL consumes it via
        // GL_ARB_gl_spirv, Vulkan natively). Absent glslang the blobs stay empty and the
        // OpenGL backend falls back to the GLSL source path.
        lDesc.VertexSpirv   = ShaderCompiler::CompileGLSLToSPIRV(EShaderStage::Vertex,   lDesc.VertexSrc,   lDesc.DebugName);
        lDesc.FragmentSpirv = ShaderCompiler::CompileGLSLToSPIRV(EShaderStage::Fragment, lDesc.FragmentSrc, lDesc.DebugName);
        return lDesc;
    }
}
