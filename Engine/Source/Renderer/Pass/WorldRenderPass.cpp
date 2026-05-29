#include "WorldRenderPass.h"

#include "World/RenderContext.h"
#include "World/IWorldSystem.h"
#include "World/World.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/RenderTarget.hpp"
#include "Renderer/Camera/CameraSubsystem.h"
#include "Renderer/Camera/ICamera.h"
#include "RHI/RenderCommand.h"
#include "Core/CoreEngineApp.h"
#include "Core/Container/TPolymorphicList.hpp"
#include "Scene/SceneManager.h"

namespace Opaax
{
    void WorldRenderPass::Execute(const RenderContext& InContext)
    {
        InContext.Target.Bind();

        RenderCommand::SetClearColor(m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a);
        RenderCommand::Clear();

        // Re-fetch the active camera every frame — never cache across a PIE swap (Lesson 17).
        ICamera& lCamera = m_App->GetSubsystem<CameraSubsystem>()->GetActiveCamera();
        Renderer2D::Begin(lCamera);

        if (m_App->GetSceneManager()->GetActiveScene())
        {
            World& lWorld = m_App->GetWorld();
            for (const auto& lSystem : TPolymorphicList<IWorldSystem>::GetAll())
            {
                lSystem->OnRender(lWorld, InContext);
            }
        }

        Renderer2D::End();

        InContext.Target.Unbind();
    }
}
