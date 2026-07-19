#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxMathTypes.h"

namespace Opaax
{
    class IRHIDevice;
    class IRenderTarget;
    class Renderer2D;
    struct RenderSystemDesc;
    struct RenderView;

    // =============================================================================
    // RenderSystem — the portable renderer orchestrator. A host owns one instance per
    //   surface. It owns the IRHIDevice, the Renderer2D batcher, and the backbuffer, and
    //   drives the frame: BeginFrame (clear) -> N x [ BeginScene(view) -> draw -> EndScene ]
    //   -> EndFrame (present). It knows NOTHING of the host — no OpaaxApplication, config,
    //   IPaths, ECS, or OPAAX_LOG. Everything crosses the boundary as plain data
    //   (RenderSystemDesc in, RenderView per frame) or the injected RenderLogFn.
    // =============================================================================
    class OPAAX_API RenderSystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        // Out-of-line — owned UniquePtr members hold forward-declared types.
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
        // Lifecycle
        // =============================================================================
    public:
        // Bring up the device against the desc's surface + build the batcher. False if the
        // backend produced no device (logged through the injected sink).
        bool Init(const RenderSystemDesc& InDesc);
        void Shutdown();

        // Host forwards surface resize here (or the adapter polls the window until an event
        // system exists). Resizes the backbuffer + device viewport.
        void Resize(Uint32 InWidth, Uint32 InHeight);

        // =============================================================================
        // Frame
        // =============================================================================
    public:
        void BeginFrame();                       // device frame open + clear the backbuffer
        void EndFrame();                         // close the pass + submit (NO present — see Present)
        void Present();                          // show the backbuffer (surface swap) — separate (S7)

        void BeginScene(const RenderView& InView); // opens a scene into the frame's command buffer
        void EndScene();                          // flushes the scene's final batch

        // =============================================================================
        // Getters - Setters
        // =============================================================================
    public:
        Renderer2D& GetRenderer2D() const noexcept { return *m_Renderer2D; }
        void        SetClearColor(const Vector4F& InColor) noexcept { m_ClearColor = InColor; }

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
