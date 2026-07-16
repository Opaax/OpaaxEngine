#include "OverlayRenderPass.h"

#include "World/RenderContext.h"
#include "World/IOverlayRenderSystem.h"
#include "World/WorldOld.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderTarget.hpp"
#include "RHI/ICommandBuffer.h"
#include "Core/CoreEngineApp.h"
#include "Core/Container/TPolymorphicList.hpp"

namespace Opaax
{
    void OverlayRenderPass::Execute(const RenderContext& InContext)
    {
        // Same target as the world pass, but Load (no clear) — composite on top.
        InContext.Cmd.BeginRenderPass(InContext.Target, ELoadOp::Load, Vector4F(0.f));

        // Pixel-space, sized to the target every frame (immune to world camera).
        m_Camera.SetViewportSize(InContext.Target.GetWidth(), InContext.Target.GetHeight());
        InContext.Renderer.Begin(m_Camera, InContext.Cmd);

        WorldOld& lWorld = m_App->GetWorld();
        for (const auto& lSystem : TPolymorphicList<IOverlayRenderSystem>::GetAll())
        {
            lSystem->OnRenderOverlay(lWorld, InContext);
        }

        InContext.Renderer.End();

        InContext.Cmd.EndRenderPass();
    }
}
