#include "WorldRenderPass.h"

#include "World/RenderContext.h"
#include "World/IWorldSystem.h"
#include "World/World.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderSubsystem.h"
#include "Renderer/Camera/ICamera.h"
#include "RHI/ICommandBuffer.h"
#include "Core/CoreEngineApp.h"
#include "Renderer/Systems/WorldSubsystem.h"
#include "Scene/SceneManager.h"

namespace Opaax
{
    void WorldRenderPass::Execute(const RenderContext& InContext)
    {
        // Open the render pass on the frame's command buffer — clears the target's color.
        InContext.Cmd.BeginRenderPass(InContext.Target, ELoadOp::Clear, m_ClearColor);

        // Re-fetch the active camera every frame — never cache across a PIE swap (Lesson 17).
        ICamera& lCamera = m_App->GetSubsystem<RenderSubsystem>()->GetActiveCamera();
        Renderer2D::Begin(lCamera, InContext.Cmd);

        if (m_App->GetSceneManager()->GetActiveScene())
        {
            World& lWorld = m_App->GetWorld();

            for (const auto& lSystem : m_App->GetSubsystem<WorldSubsystem>()->GetSystems())
            {
                lSystem->OnRender(lWorld, InContext);
            }
        }

        Renderer2D::End();

        InContext.Cmd.EndRenderPass();
    }
}
