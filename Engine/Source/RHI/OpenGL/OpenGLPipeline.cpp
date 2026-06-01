#include "OpenGLPipeline.h"

#include "RHI/Shader.h"

#define GLAD_APIENTRY
#include <glad/glad.h>

namespace Opaax
{
    // NOTE: the IPipeline::Create factory dispatch lives in RHI/BackendFactory.cpp.

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
            case EBlendMode::None:
                glDisable(GL_BLEND);
                break;
        }
    }
}
