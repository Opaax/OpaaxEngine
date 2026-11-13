#pragma once

#include "RHI/UniformBuffer.h"

namespace Opaax
{
    /**
     * OpenGL uniform buffer. Made for one binding point; the bind group that holds it puts it
     * there when it is bound.
     */
    class OpenGLUniformBuffer final : public IUniformBuffer
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
        // Functions
        // =============================================================================
    public:
        /** The binding point it was made for. */
        Uint32 GetBinding() const noexcept { return m_Binding; }

        /** Puts the buffer at its binding point. Called by OpenGLBindGroup::Bind. */
        void Bind() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        Uint32 m_RendererID = 0;
        Uint32 m_Binding    = 0;
    };

} // namespace Opaax
