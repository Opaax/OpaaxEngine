#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "RHI/Buffer.h"   // BufferLayout

namespace Opaax
{
    class IShader;

    // =============================================================================
    // Pipeline state enums
    // =============================================================================
    enum class EBlendMode
    {
        None,
        Alpha   // src-alpha / one-minus-src-alpha (standard 2D transparency)
    };

    enum class EPrimitiveTopology
    {
        Triangles
    };

    // =============================================================================
    // PipelineDesc
    // =============================================================================
    /**
     * @struct PipelineDesc
     *
     * Backend-neutral description of a graphics pipeline — the input to IPipeline::Create.
     * Bundles the shader, vertex input layout, and fixed-function state that a command-buffer
     * backend must bake into an immutable pipeline object up front.
     *
     * NOTE: VertexLayout is consumed by backends that need explicit vertex-input state in the
     *   pipeline (Vulkan). The OpenGL backend already encodes the layout in the bound VAO, so
     *   its pipeline impl ignores this field.
     */
    struct PipelineDesc
    {
        IShader*           Shader    = nullptr;                     // not owned — must outlive the pipeline
        BufferLayout       VertexLayout;                           // vertex input (Vulkan); GL uses the VAO
        EBlendMode         Blend     = EBlendMode::Alpha;
        EPrimitiveTopology Topology  = EPrimitiveTopology::Triangles;
        const char*        DebugName = "Pipeline";
    };

    // =============================================================================
    // IPipeline
    // =============================================================================
    /**
     * @interface IPipeline
     *
     * Backend-agnostic graphics pipeline state object. Consumers (Renderer2D) hold a
     * UniquePtr<IPipeline> and bind it on the command buffer. The concrete impl is selected
     * by IPipeline::Create, defined in the active backend's TU (OpenGLPipeline.cpp today) —
     * mirrors the IShader/IVertexArray factory pattern.
     */
    class OPAAX_API IPipeline
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IPipeline() = default;

        // =============================================================================
        // Factory
        // =============================================================================
    public:
        static UniquePtr<IPipeline> Create(const PipelineDesc& InDesc);
    };

} // namespace Opaax
