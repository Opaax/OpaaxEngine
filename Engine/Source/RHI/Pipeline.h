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
        Alpha,     // src-alpha / one-minus-src-alpha (standard 2D transparency)
        Additive   // one / one (light, bloom)
    };

    enum class EPrimitiveTopology
    {
        Triangles
    };

    // =============================================================================
    // PipelineDesc
    // =============================================================================
    /**
     * A graphics pipeline description: shader, vertex layout and fixed-function state.
     * VertexLayout is for backends that need it in the pipeline (Vulkan); OpenGL uses the VAO.
     */
    struct PipelineDesc
    {
        IShader*           Shader    = nullptr;                     // not owned, must outlive the pipeline
        BufferLayout       VertexLayout;                           // vertex input (Vulkan); GL uses the VAO
        EBlendMode         Blend     = EBlendMode::Alpha;
        EPrimitiveTopology Topology  = EPrimitiveTopology::Triangles;
        const char*        DebugName = "Pipeline";
    };

    // =============================================================================
    // IPipeline
    // =============================================================================
    /**
     * Graphics pipeline state. Created by IRHIDevice::CreatePipeline, bound on the command buffer.
     */
    class IPipeline
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IPipeline() = default;

        // Created via IRHIDevice::CreatePipeline.
    };

} // namespace Opaax
