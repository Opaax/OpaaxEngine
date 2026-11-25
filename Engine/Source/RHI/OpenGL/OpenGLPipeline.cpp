#include "RHI/OpenGL/OpenGLPipeline.h"

#include "RHI/BlendState.h"
#include "RHI/Shader.h"

#include <glad/glad.h>

namespace Opaax
{
    namespace
    {
        GLenum ToGL(const EBlendFactor InFactor)
        {
            switch (InFactor)
            {
                case EBlendFactor::Zero:             return GL_ZERO;
                case EBlendFactor::One:              return GL_ONE;
                case EBlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
                case EBlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            }
            return GL_ONE;
        }
    }

    OpenGLPipeline::OpenGLPipeline(const PipelineDesc& InDesc)
        : m_Shader(InDesc.Shader), m_Blend(InDesc.Blend)
    {
    }

    void OpenGLPipeline::Apply() const
    {
        if (m_Shader) { m_Shader->Bind(); }

        const BlendFactors lBlend = MakeBlendFactors(m_Blend);
        if (!lBlend.bEnabled)
        {
            glDisable(GL_BLEND);
            return;
        }

        glEnable(GL_BLEND);
        glBlendFuncSeparate(ToGL(lBlend.SrcColor), ToGL(lBlend.DstColor), ToGL(lBlend.SrcAlpha), ToGL(lBlend.DstAlpha));
    }
}
