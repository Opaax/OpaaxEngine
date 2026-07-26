#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/Maths/MathTypes.h"
#include "Application/Services/ILogger.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(RenderSystem)
    
    class IRHIDevice;
    class IRenderTarget;
    class Renderer2D;
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
        bool IsValidBackbuffer()    const noexcept { return m_Backbuffer.get()  != nullptr; }

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
        // Getters - Setters
    
    public:
        /***/
        Renderer2D& GetRenderer2D() const noexcept { return *m_Renderer2D; }
        
        /***/
        void        SetClearColor(const Vector4F& InColor) noexcept { m_ClearColor = InColor; }
        
        /**
         * @return The window-surface target. The default primary target when no offscreen target is set.
         */
        IRenderTarget& GetBackbuffer() const noexcept { return *m_Backbuffer; }
        // End Getters - Setters
        // =============================================================================
        
        // =============================================================================
        // Members
        // =============================================================================
    private:
        UniquePtr<IRHIDevice>    m_Device;
        UniquePtr<Renderer2D>    m_Renderer2D;
        UniquePtr<IRenderTarget> m_Backbuffer;   // DefaultRenderTarget (window surface)
        Vector4F                 m_ClearColor{0.f, 0.f, 0.f, 1.f};
    };
}
