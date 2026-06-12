#include "WorldRenderSystem.h"

#include "Core/Config/EngineConfig.h"
#include "World/World.h"
#include "ECS/Components/SpriteComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/TransformInterpolationComponent.h"
#include "ECS/Hierarchy.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/Texture2D.h"
#include "World/RenderContext.h"

#include <cmath>

namespace Opaax
{
    namespace
    {
        // Shortest-arc angle lerp. Wraps the delta into (-pi, pi] so a body whose Box2D
        // angle wrapped at +/-pi interpolates the short way instead of spinning backward
        // through a full turn.
        float LerpAngleShortest(float InFrom, float InTo, float InAlpha)
        {
            constexpr float kPi    = 3.14159265359f;
            constexpr float kTwoPi = 6.28318530718f;

            float lDelta = std::fmod(InTo - InFrom + kPi, kTwoPi);
            if (lDelta < 0.f) { lDelta += kTwoPi; }
            lDelta -= kPi;
            return InFrom + lDelta * InAlpha;
        }
    }

    void WorldRenderSystem::OnRender(World& InWorld, const RenderContext& InContext)
    {
        // Fixed-step interpolation: physics/mover write the Transform at a stable 60 Hz, so
        // at higher render rates the raw pose repeats across frames (visible stepping). For
        // entities the fixed-step writers tagged with a TransformInterpolationComponent we
        // lerp prev->current by the frame's alpha — DISPLAY ONLY, the ECS Transform stays raw.
        const bool  bInterpolate = EngineConfig::RenderInterpolation();
        const float lAlpha       = static_cast<float>(InContext.Alpha);

        // PERF: Each<A,B> picks the smaller storage — avoids scanning all transforms
        // when sprite count is low (common during early scenes).
        InWorld.Each<ECS::TransformComponent, ECS::SpriteComponent>(
            [&InWorld, bInterpolate, lAlpha](EntityID InEntity, ECS::TransformComponent& /*InTransform*/, ECS::SpriteComponent& InSprite)
            {
                if (!InSprite.Visible || !InSprite.Texture.IsValid())
                {
                    return;
                }

                ECS::Hierarchy::WorldTransform lWT =
                    ECS::Hierarchy::GetWorldTransform(InWorld, InEntity);

                // NOTE: interpolation is applied at the world pose, which is correct for the
                //   root-level physics entities this targets (physics writes world coords into
                //   the local Transform). Parented physics bodies are out of scope (see P7).
                if (bInterpolate)
                {
                    if (const ECS::TransformInterpolationComponent* lInterp =
                            InWorld.GetComponent<ECS::TransformInterpolationComponent>(InEntity))
                    {
                        lWT.Position = lInterp->PrevPosition + (lWT.Position - lInterp->PrevPosition) * lAlpha;
                        lWT.Rotation = LerpAngleShortest(lInterp->PrevRotation, lWT.Rotation, lAlpha);
                    }
                }

                const Vector2F lDrawSize = lWT.Scale * InSprite.Size;

                Renderer2D::DrawSprite(
                    lWT.Position,
                    lDrawSize,
                    InSprite.Texture,
                    InSprite.UVMin,
                    InSprite.UVMax,
                    InSprite.Color,
                    lWT.Rotation,
                    InSprite.Layer,
                    InSprite.OrderInLayer
                );
            });
    }
}
