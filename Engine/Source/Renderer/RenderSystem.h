#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Core/Log/Logger.h"
#include "RHI/ICommandBuffer.h"   // ELoadOp

namespace Opaax
{
    OPAAX_LOG_CATEGORY(RenderSystem);
    
    class IRHIDevice;
    class IFramebuffer;
    class IRenderTarget;
    class ITexture2D;
    class Renderer2D;
    struct FramebufferSpec;
    struct RenderSystemDesc;
    struct RenderView;

    // =============================================================================
    // RenderSystem — the renderer. A host owns one per surface. Owns the device, the Renderer2D
    //   batcher and the backbuffer, and runs the frame:
    //     BeginFrame -> N x [ BeginPass(target, view) -> draw -> EndPass ] -> EndFrame -> Present
    //   A pass renders into an IRenderTarget (backbuffer or offscreen framebuffer). Only the
    //   backbuffer is presented. Takes host state as plain data (RenderSystemDesc, RenderView).
    // =============================================================================
    class RenderSystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        RenderSystem();
        ~RenderSystem();

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        RenderSystem(const RenderSystem&)            = delete;
        RenderSystem& operator=(const RenderSystem&) = delete;
        RenderSystem(RenderSystem&&)                 = delete;
        RenderSystem& operator=(RenderSystem&&)      = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
    private:
        bool IsValidDevice()        const noexcept { return m_Device.get()      != nullptr; }
        bool IsValidRenderer2D()    const noexcept { return m_Renderer2D.get()  != nullptr; }

        // =============================================================================
        // Lifecycle
    public:
        
        /**
         * Creates the device on the desc's surface, and the batcher.
         * @return False if the backend gave no device
         */
        bool Init(const RenderSystemDesc& InDesc);
        
        void Shutdown();
        
        // End Lifecycle
        // =============================================================================

        // =============================================================================
        // Frame
    public:
        /** Opens the device frame (no pass yet). */
        void BeginFrame();
        /** Submits the frame (does not present). */
        void EndFrame();
        /** Shows the backbuffer. */
        void Present();

        /**
         * Opens a pass into InTarget: binds it, clears it (or keeps it, e.g. for UI on top),
         * then starts the batcher with InView. Draws until EndPass go into this pass.
         */
        void BeginPass(IRenderTarget& InTarget, const RenderView& InView, ELoadOp InLoadOp = ELoadOp::Clear);
        /** Flushes the batch and closes the pass. */
        void EndPass();
        
        /**
         * Surface resize: resizes the backbuffer and viewport.
         */
        void Resize(Uint32 InWidth, Uint32 InHeight);
        
        // End Frame
        // =============================================================================

        // =============================================================================
        // Resources
    public:
        /**
         * Creates an offscreen framebuffer. Release it before the device is destroyed.
         * @param InSpec Size, and whether it has depth/stencil
         * @return Null if there is no device
         */
        TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec);

        /**
         * Creates a texture from decoded pixels. Release it before the device is destroyed.
         * @param InPixels Tightly packed, InWidth * InHeight * InChannels bytes. Copied.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8
         * @return Null if there is no device
         */
        TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        // End Resources
        // =============================================================================

        // =============================================================================
        // Getters - Setters
    
    public:
        Renderer2D& GetRenderer2D() const noexcept { return *m_Renderer2D; }

        /**
         * @return The window surface target
         */
        IRenderTarget& GetBackbuffer() const noexcept { return *m_Backbuffer; }

        /**
         * GPU time of a recent frame, in milliseconds.
         * @return Negative without a device, or before the first result
         */
        double GetGpuFrameTimeMs() const;

        /** Clear colour of passes that clear. Applied from the next frame. */
        void SetClearColor(const Vector4F& InColor) noexcept { m_ClearColor = InColor; }
        // End Getters - Setters
        // =============================================================================
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IRHIDevice>    m_Device;
        TUniquePtr<Renderer2D>    m_Renderer2D;
        TUniquePtr<IRenderTarget> m_Backbuffer;   // window surface
        Vector4F                 m_ClearColor{0.f, 0.f, 0.f, 1.f};
    };
}
