#include "OverlayRenderPass.h"

#include "World/RenderContext.h"
#include "World/IOverlayRenderSystem.h"
#include "World/World.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderTarget.hpp"
#include "Core/CoreEngineApp.h"
#include "Core/Container/TPolymorphicList.hpp"

namespace Opaax
{
    void OverlayRenderPass::Execute(const RenderContext& InContext)
    {
        // Same target as the world pass, but NO clear — composite on top.
        InContext.Target.Bind();

        // Pixel-space, sized to the target every frame (immune to world camera).
        m_Camera.SetViewportSize(InContext.Target.GetWidth(), InContext.Target.GetHeight());
        Renderer2D::Begin(m_Camera);

        World& lWorld = m_App->GetWorld();
        for (const auto& lSystem : TPolymorphicList<IOverlayRenderSystem>::GetAll())
        {
            lSystem->OnRenderOverlay(lWorld, InContext);
        }

        Renderer2D::End();

        InContext.Target.Unbind();
    }
}
