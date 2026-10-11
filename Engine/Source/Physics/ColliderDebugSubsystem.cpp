#include "Physics/ColliderDebugSubsystem.h"

#include "Core/Maths/Maths.h"   // DegreesToRadians
#include "Core/Profiling/Profiler.h"
#include "Renderer/DebugDraw.h"
#include "Physics/Components/ColliderComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** Green blocks, yellow passes through (like Unreal). */
        constexpr Vector4F kSolidColor{ 0.30f, 0.90f, 0.35f, 0.85f };
        constexpr Vector4F kOverlapColor{ 0.95f, 0.85f, 0.25f, 0.85f };

        constexpr float kThickness = 2.f;

        /** Rotates a local offset into world space, by the entity's rotation. */
        Vector2F RotateOffset(const Vector2F& InOffset, const float InRadians) noexcept
        {
            if (InOffset.x == 0.f && InOffset.y == 0.f)
            {
                return InOffset;
            }

            const float lCos = Maths::Cos(InRadians);
            const float lSin = Maths::Sin(InRadians);

            return { InOffset.x * lCos - InOffset.y * lSin,
                     InOffset.x * lSin + InOffset.y * lCos };
        }
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool ColliderDebugSubsystem::Startup()
    {
        // Also runs in Edit worlds; entities may not exist yet.
        return true;
    }

    void ColliderDebugSubsystem::Shutdown()
    {
    }

    // =========================================================================
    // Tick
    // =========================================================================
    void ColliderDebugSubsystem::Update(double)
    {
        OPAAX_STAT_SCOPE("ColliderDebug");

        // Checked once: a hidden channel costs a lookup, not a walk of every entity.
        if (!m_Context->Debug.IsChannelEnabled(DebugChannels::Physics))
        {
            m_LastDrawn = 0;
            return;
        }

        Uint64 lDrawn = 0;

        World& lWorld = m_Context->OwningWorld;

        lWorld.Each<ColliderComponent, TransformComponent>(
            [this, &lWorld, &lDrawn](EntityID InEntity, ColliderComponent& InCollider, TransformComponent&)
            {
                // World pose.
                DrawCollider(InCollider, EntityHierarchy::WorldTransform(Entity{ InEntity, &lWorld }));
                ++lDrawn;
            });

        m_LastDrawn = lDrawn;
    }

    // =========================================================================
    // Drawing
    // =========================================================================
    void ColliderDebugSubsystem::DrawCollider(const ColliderComponent& InCollider,
                                              const TransformComponent& InTransform)
    {
        DebugDraw& lDebug = m_Context->Debug;

        const float    lRadians = Maths::DegreesToRadians(InTransform.Rotation);
        const Vector2F lCenter  = InTransform.Position + RotateOffset(InCollider.Offset, lRadians);
        const Vector4F lColor   = ColorFor(InCollider);

        switch (InCollider.Shape)
        {
            case EColliderShape::Circle:
            {
                lDebug.DrawCircle(lCenter, InCollider.Radius, lColor, kThickness,
                                  ERenderLayer::Debug, DebugChannels::Physics);
                break;
            }

            case EColliderShape::Capsule:
            {
                // The caps sit a radius in from each end, like MakeShapeDesc builds them.
                const float lHalfSpan = Maths::Max(0.f, InCollider.Size.y * 0.5f - InCollider.Radius);
                const Vector2F lAxis  = RotateOffset({ 0.f, lHalfSpan }, lRadians);

                lDebug.DrawCapsule(lCenter - lAxis, lCenter + lAxis, InCollider.Radius, lColor,
                                   kThickness, ERenderLayer::Debug, DebugChannels::Physics);
                break;
            }

            case EColliderShape::Box:
            default:
            {
                // One hollow quad, rotated with the entity.
                lDebug.DrawBox(lCenter, InCollider.Size, lColor, kThickness,
                               ERenderLayer::Debug, DebugChannels::Physics, lRadians);
                break;
            }
        }
    }

    Vector4F ColliderDebugSubsystem::ColorFor(const ColliderComponent& InCollider)
    {
        return InCollider.Mode == EColliderMode::Overlap ? kOverlapColor : kSolidColor;
    }
}
