#include "RHI/OpenGL/OpenGLPipeline.h"

#include "RHI/Shader.h"

#include <glad/glad.h>

namespace Opaax
{

    OpenGLPipeline::OpenGLPipeline(const PipelineDesc& InDesc)
        : m_Shader(InDesc.Shader), m_Blend(InDesc.Blend)
    {
    }

    void OpenGLPipeline::Apply() const
    {
        if (m_Shader) { m_Shader->Bind(); }

        switch (m_Blend)
        {
            case EBlendMode::Alpha:
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                break;
            case EBlendMode::Additive:
                glEnable(GL_BLEND);
                glBlendFunc(GL_ONE, GL_ONE);
                break;
            case EBlendMode::None:
                glDisable(GL_BLEND);
                break;
        }
    }
}
