#include "World/Systems/ColliderDebugSubsystem.h"

#include "Core/Maths/Maths.h"   // DegreesToRadians — the transform authors degrees
#include "Core/Profiling/Profiler.h"
#include "Renderer/DebugDraw.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Entity/EntityHierarchy.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** Unreal's convention, near enough to be unsurprising: green blocks, yellow passes through. */
        constexpr Vector4F kSolidColor{ 0.30f, 0.90f, 0.35f, 0.85f };
        constexpr Vector4F kOverlapColor{ 0.95f, 0.85f, 0.25f, 0.85f };

        constexpr float kThickness = 2.f;

        /** Rotate a local offset into world space around the entity's own rotation. */
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
        // No ShouldCreate, so this runs in an Edit world too — which is the whole point, and also
        // why it must not assume any entity exists yet (WS7).
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

        // Asking ONCE rather than per collider: the channel cannot change mid-tick, and a silenced
        // channel should cost a lookup, not a walk of every entity in the world.
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
                // WORLD, where the body actually is (§HR).
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
                // The caps sit a radius in from each end, exactly as MakeShapeDesc builds them —
                // an outline that used the full half-height would be taller than the shape it
                // annotates, which is the kind of wrong that looks almost right.
                const float lHalfSpan = Maths::Max(0.f, InCollider.Size.y * 0.5f - InCollider.Radius);
                const Vector2F lAxis  = RotateOffset({ 0.f, lHalfSpan }, lRadians);

                lDebug.DrawCapsule(lCenter - lAxis, lCenter + lAxis, InCollider.Radius, lColor,
                                   kThickness, ERenderLayer::Debug, DebugChannels::Physics);
                break;
            }

            case EColliderShape::Box:
            default:
            {
                // One hollow quad, carrying the entity's rotation — which is why DebugBox grew a
                // RotationRad: the ramp in the physics test map is authored at -25 degrees.
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
