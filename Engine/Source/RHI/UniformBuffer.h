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
     * TUniquePtr<IUniformBuffer> and write with SetData each frame.
     *
     * The concrete is created via IRHIDevice::CreateUniformBuffer (OpenGLUniformBuffer today).
     */
    class OPAAX_API IUniformBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        virtual ~IUniformBuffer() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        // Created via IRHIDevice::CreateUniformBuffer.
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
