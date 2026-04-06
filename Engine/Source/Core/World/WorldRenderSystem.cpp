#include "WorldRenderSystem.h"

#include "ECS/BaseComponents.hpp"
#include "Renderer/Renderer2D.h"

namespace Opaax::ECS
{
    void WorldRenderSystem::Render(World& InWorld)
    {
        // PERF: Iterates only entities with BOTH Transform and Sprite.
        //   Each<A,B> picks the smaller storage — avoids scanning all transforms
        //   if sprite count is low (common during early scenes).
        InWorld.Each<TransformComponent, SpriteComponent>(
            [](EntityID /*InEntity*/, TransformComponent& InTransform, SpriteComponent& InSprite)
            {
                if (!InSprite.Visible || !InSprite.Texture.IsValid()) { return; }

                Renderer2D::DrawSprite(
                    InTransform.Position,
                    InTransform.Scale,
                    InSprite.Texture,
                    InSprite.UVMin,
                    InSprite.UVMax,
                    InSprite.Color
                );
            });
    }

} // namespace Opaax::ECS