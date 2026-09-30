#pragma once

#include "RHI/Pipeline.h"

namespace Opaax
{
    class IShader;

    /**
     * OpenGL IPipeline: stores the shader and blend state, applied when bound.
     * The vertex layout comes from the VAO, so PipelineDesc::VertexLayout is unused.
     */
    class OPAAX_API OpenGLPipeline final : public IPipeline
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        explicit OpenGLPipeline(const PipelineDesc& InDesc);
        ~OpenGLPipeline() override = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        // Called by OpenGLCommandBuffer::BindPipeline: binds the shader and applies the blend state.
        void Apply() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IShader*   m_Shader = nullptr;          // not owned
        EBlendMode m_Blend  = EBlendMode::Alpha;
    };
}
