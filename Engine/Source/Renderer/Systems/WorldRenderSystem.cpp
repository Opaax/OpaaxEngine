#include "WorldRenderSystem.h"

#include "Core/World/World.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Hierarchy.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"
#include "World/RenderContext.h"

namespace Opaax
{
    void WorldRenderSystem::OnRender(World& InWorld, const RenderContext& /*InContext*/)
    {
        // PERF: Each<A,B> picks the smaller storage — avoids scanning all transforms
        // when sprite count is low (common during early scenes).
        InWorld.Each<ECS::TransformComponent, ECS::SpriteComponent>(
            [&InWorld](EntityID InEntity, ECS::TransformComponent& /*InTransform*/, ECS::SpriteComponent& InSprite)
            {
                if (!InSprite.Visible || !InSprite.Texture.IsValid())
                {
                    return;
                }

                const ECS::Hierarchy::WorldTransform lWT =
                    ECS::Hierarchy::GetWorldTransform(InWorld, InEntity);

                const Vector2F lDrawSize = lWT.Scale * InSprite.Size;

                Renderer2D::DrawSprite(
                    lWT.Position,
                    lDrawSize,
                    InSprite.Texture,
                    InSprite.UVMin,
                    InSprite.UVMax,
                    InSprite.Color,
                    lWT.Rotation
                );
            });
    }
}
