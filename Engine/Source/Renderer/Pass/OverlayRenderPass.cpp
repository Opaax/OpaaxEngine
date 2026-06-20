#include "OverlayRenderPass.h"

#include "World/RenderContext.h"
#include "World/IOverlayRenderSystem.h"
#include "World/World.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderTarget.hpp"
#include "RHI/ICommandBuffer.h"
#include "Core/CoreEngineApp.h"
#include "Renderer/RenderSubsystem.h"

namespace Opaax
{
    void OverlayRenderPass::Execute(const RenderContext& InContext)
    {
        // Same target as the world pass, but Load (no clear) — composite on top.
        InContext.Cmd.BeginRenderPass(InContext.Target, ELoadOp::Load, Vector4F(0.f));

        // Pixel-space, sized to the target every frame (immune to world camera).
        m_Camera.SetViewportSize(InContext.Target.GetWidth(), InContext.Target.GetHeight());
        Renderer2D::Begin(m_Camera, InContext.Cmd);

        World& lWorld = m_App->GetWorld();
        for (const auto& lSystem : m_App->GetSubsystem<RenderSubsystem>()->GetOverlaySystems())
        {
            lSystem->OnRenderOverlay(lWorld, InContext);
        }

        Renderer2D::End();

        InContext.Cmd.EndRenderPass();
    }
}
