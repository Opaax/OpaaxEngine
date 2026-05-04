#include "WorldRenderSystem.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Hierarchy.h"
#include "Renderer/Renderer2D.h"

namespace Opaax
{
    void WorldRenderSystem::Render(World& InWorld)
    {
        // PERF: Iterates only entities with BOTH Transform and Sprite.
        //   Each<A,B> picks the smaller storage — avoids scanning all transforms
        //   if sprite count is low (common during early scenes).
        InWorld.Each<ECS::TransformComponent, ECS::SpriteComponent>(
            [&InWorld](EntityID InEntity, ECS::TransformComponent& /*InTransform*/, ECS::SpriteComponent& InSprite)
            {
                if (!InSprite.Visible || !InSprite.Texture.IsValid())
                {
                    return;
                }

                // Compound with any parent chain. Local-only sprites still get an identity walk.
                const ECS::Hierarchy::WorldTransform lWT =
                    ECS::Hierarchy::GetWorldTransform(InWorld, InEntity);

                // Final draw size = sprite intrinsic size × hierarchical scale.
                // Component-wise multiply (Hadamard) — glm::vec2 operator* does this.
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

} // namespace Opaax::ECS
