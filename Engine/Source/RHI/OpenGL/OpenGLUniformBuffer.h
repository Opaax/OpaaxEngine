#pragma once

#include "RHI/UniformBuffer.h"

namespace Opaax
{
    /**
     * @class OpenGLUniformBuffer
     *
     * Implement IUniformBuffer for OpenGL via DSA (glCreateBuffers / glNamedBuffer*).
     * Bound once to its binding point at construction (glBindBufferBase).
     */
    class OPAAX_API OpenGLUniformBuffer final : public IUniformBuffer
    {
        // =============================================================================
        // CTOR - DTOR
        // =============================================================================
    public:
        OpenGLUniformBuffer(Uint32 InSize, Uint32 InBinding);
        ~OpenGLUniformBuffer() override;

        // =============================================================================
        // Copy - Delete
        // =============================================================================
    public:
        OpenGLUniformBuffer(const OpenGLUniformBuffer&)            = delete;
        OpenGLUniformBuffer& operator=(const OpenGLUniformBuffer&) = delete;

        // =============================================================================
        // Move
        // =============================================================================
    public:
        OpenGLUniformBuffer(OpenGLUniformBuffer&&)                 = default;
        OpenGLUniformBuffer& operator=(OpenGLUniformBuffer&&)      = default;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IUniformBuffer interface
    public:
        void SetData(const void* InData, Uint32 InSize, Uint32 InOffset = 0) override;
        //~End IUniformBuffer interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;
    };

} // namespace Opaax
