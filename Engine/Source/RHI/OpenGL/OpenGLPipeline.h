#pragma once

#include "RHI/Pipeline.h"

namespace Opaax
{
    class IShader;

    /**
     * @class OpenGLPipeline
     *
     * OpenGL IPipeline: there is no immutable pipeline object in GL, so this just stores the
     * shader + blend state and applies them when the command buffer binds it (Apply). The
     * vertex layout is carried by the bound VAO on GL, so PipelineDesc::VertexLayout is unused
     * here (it exists for command-buffer backends that bake vertex input into the pipeline).
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
        // Backend-internal: called by OpenGLCommandBuffer::BindPipeline. Binds the shader
        // program and applies the blend state.
        void Apply() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        IShader*   m_Shader = nullptr;          // not owned
        EBlendMode m_Blend  = EBlendMode::Alpha;
    };
}
