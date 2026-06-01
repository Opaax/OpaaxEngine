#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // IUniformBuffer
    // =============================================================================
    /**
     * @interface IUniformBuffer
     *
     * Backend-agnostic uniform buffer bound to a fixed binding point. Replaces the
     * name-based default-block uniform path, which SPIR-V GLSL forbids — shaders read
     * `layout(std140, binding = N) uniform Block { ... }`. Consumers hold a
     * UniquePtr<IUniformBuffer> and write with SetData each frame.
     *
     * The concrete is selected by IUniformBuffer::Create, defined in the active
     * backend's TU (OpenGLUniformBuffer.cpp) — mirrors the IVertexBuffer factory.
     */
    class OPAAX_API IUniformBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IUniformBuffer() = default;

        // =============================================================================
        // Statics
        // =============================================================================

        /**
         * @param InSize    byte size of the block (std140 layout)
         * @param InBinding binding point the shader declares (layout(binding = N))
         * @return owning uniform buffer
         */
        static UniquePtr<IUniformBuffer> Create(Uint32 InSize, Uint32 InBinding);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Upload bytes into the block.
         * @param InData   source bytes
         * @param InSize   number of bytes to upload
         * @param InOffset byte offset into the block (default 0)
         */
        virtual void SetData(const void* InData, Uint32 InSize, Uint32 InOffset = 0) = 0;
    };

} // namespace Opaax
