#pragma once

namespace Opaax
{
    class IRenderTarget;
    class ICommandBuffer;
    class Renderer2D;

    // Render-side context handed to every IRenderPass::Execute and forwarded to each
    // IWorldSystem::OnRender. Kept minimal on purpose — add fields only when a concrete
    // pass or system needs them.
    //
    // NOTE: the active camera is NO LONGER here — it is a per-pass concern (each pass calls
    //   Renderer.Begin with its own camera). This removes the old single-camera assumption
    //   and the vestigial RenderContext::Camera (M7 Step 3).
    struct RenderContext
    {
        IRenderTarget&  Target;   // the render target this pass draws into (via Cmd.BeginRenderPass)
        ICommandBuffer& Cmd;      // the frame's recorder — passes record their draws here
        double          Alpha;    // physics-step interpolation alpha [0,1]
        Renderer2D&     Renderer; // the frame's batch renderer — passes/systems draw through this
    };
}
