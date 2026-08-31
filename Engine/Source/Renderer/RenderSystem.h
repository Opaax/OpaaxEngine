#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Application/Services/ILogger.h"

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
    // RenderSystem — the renderer orchestrator. A host owns one instance per surface. It
    //   owns the IRHIDevice, the Renderer2D batcher, and the backbuffer, and drives the
    //   frame: BeginFrame -> N x [ BeginPass(target,view) -> draw -> EndPass ] -> EndFrame
    //   -> Present. A "pass" renders into an IRenderTarget (the backbuffer, or an offscreen
    //   FBO for the editor viewport) with a RenderView — "scene" is retired vocabulary (F2).
    //   Only the backbuffer is ever presented. Host state crosses the boundary as plain data
    //   (RenderSystemDesc in, RenderView per frame) — no OpaaxApplication / config / IPaths /
    //   ECS reach-back. It logs through the engine logger (OPAAX_LOG), same as the rest of the RHI.
    // =============================================================================
    class OPAAX_API RenderSystem
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
         * Bring up the device against the desc's surface + build the batcher.
         * @param InDesc 
         * @return False if the backend produced no device.
         */
        bool Init(const RenderSystemDesc& InDesc);
        
        /***/
        void Shutdown();
        
        // End Lifecycle
        // =============================================================================

        // =============================================================================
        // Frame
    public:
        /** device frame open (no pass yet) */
        void BeginFrame();
        /** Submit the frame (NO present — see Present) */
        void EndFrame();
        /** show the backbuffer (surface swap)*/
        void Present();

        /**
         * Open a render pass into InTarget (backbuffer or offscreen FBO): 
         *      binds the target + clears
         *      then opens the batcher on InView.
         * Draws issued until EndPass record into this pass.
         * @param InTarget 
         * @param InView 
         */
        
        void BeginPass(IRenderTarget& InTarget, const RenderView& InView);
        /** flush the batch, then close the pass */
        void EndPass();
        
        /**
         * Host forwards surface resize here.
         * Resizes the backbuffer + device viewport.
         * @param InWidth 
         * @param InHeight 
         */
        void Resize(Uint32 InWidth, Uint32 InHeight);
        
        // End Frame
        // =============================================================================

        // =============================================================================
        // Resources
    public:
        /**
         * Build an offscreen framebuffer on the device (F2a — GPU resources are device-created).
         * The CALLER owns it and must release it before the device dies.
         * @param InSpec Size + whether a depth/stencil attachment is wanted.
         * @return nullptr if the render core has no device (Init failed or never ran).
         */
        TUniquePtr<IFramebuffer> CreateFramebuffer(const FramebufferSpec& InSpec);

        /**
         * Build a texture on the device from pixels the caller decoded (F2a). The CALLER owns it
         * and must release it before the device dies.
         * @param InPixels Tightly packed, InWidth * InHeight * InChannels bytes; borrowed.
         * @param InChannels 4 = RGBA8, 3 = RGB8, 1 = R8 coverage.
         * @return nullptr if the render core has no device (Init failed or never ran).
         */
        TUniquePtr<ITexture2D> CreateTexture(const void* InPixels, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

        // End Resources
        // =============================================================================

        // =============================================================================
        // Getters - Setters
    
    public:
        /***/
        Renderer2D& GetRenderer2D() const noexcept { return *m_Renderer2D; }

        /**
         * @return The window-surface target. The default primary target when no offscreen target is set.
         */
        IRenderTarget& GetBackbuffer() const noexcept { return *m_Backbuffer; }

        /**
         * The device's own GPU timing for a recent frame, in milliseconds (④ S3).
         * @return Negative with no device, and until the first query result lands.
         */
        double GetGpuFrameTimeMs() const;
        // End Getters - Setters
        // =============================================================================
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        TUniquePtr<IRHIDevice>    m_Device;
        TUniquePtr<Renderer2D>    m_Renderer2D;
        TUniquePtr<IRenderTarget> m_Backbuffer;   // DefaultRenderTarget (window surface)
        Vector4F                 m_ClearColor{0.f, 0.f, 0.f, 1.f};
    };
}
