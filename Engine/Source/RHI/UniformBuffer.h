#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // IUniformBuffer
    // =============================================================================
    /**
     * Uniform buffer made for one binding point (layout(std140, binding = N) uniform Block).
     * Created by IRHIDevice::CreateUniformBuffer; written with SetData. A shader reads it in the
     * passes whose bind group holds it (IBindGroup::SetUniformBuffer): creating it binds nothing.
     */
    class IUniformBuffer
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
         * Uploads bytes into the block.
         * @param InData   Source bytes
         * @param InSize   Byte count
         * @param InOffset Byte offset in the block
         */
        virtual void SetData(const void* InData, Uint32 InSize, Uint32 InOffset = 0) = 0;
    };

} // namespace Opaax
