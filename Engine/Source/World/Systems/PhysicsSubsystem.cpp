#include "World/Systems/PhysicsSubsystem.h"

#include "Core/Events/EventBus.h"
#include "Core/Maths/Maths.h"   // DegreesToRadians — the transform authors degrees, the seam takes radians
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Config/EngineConfigData.h"
#include "Engine/Subsystems/EventBus/EngineEventBus.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/PhysicsAPI.h"
#include "Physics/PhysicsEvents.h"
#include "World/Components/ColliderComponent.h"
#include "World/Components/RigidbodyComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /**
         * Entity bits -> body user-data, OFFSET BY ONE so a valid entity never encodes as 0 —
         * which is what the seam reserves for "unresolved" (a stale shape in an end-touch event).
         * Entity 0 is a perfectly ordinary entity, so without the offset it would be indistinguishable
         * from no entity at all.
         */
        Uint64 ToUserData(EntityID InEntity) noexcept
        {
            return static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1ull;
        }

        Uint32 EntityBits(EntityID InEntity) noexcept { return static_cast<Uint32>(InEntity); }

        /** The inverse of ToUserData. 0 is the seam's "unresolved", and yields ENTITY_NONE. */
        EntityID FromUserData(Uint64 InUserData) noexcept
        {
            return InUserData == 0
                       ? ENTITY_NONE
                       : static_cast<EntityID>(static_cast<Uint32>(InUserData - 1ull));
        }
    }

    // =========================================================================
    // Base implementation
    // =========================================================================
    bool PhysicsSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool PhysicsSubsystem::Startup()
    {
        const PhysicsSettings& lSettings = m_Context->Config.Physics;

        PhysicsWorldDesc lDesc;
        lDesc.Gravity             = lSettings.Gravity;
        lDesc.LengthUnitsPerMeter = lSettings.LengthUnitsPerMeter;
        lDesc.SubStepCount        = lSettings.SubStepCount;

        m_SubStepCount = lSettings.SubStepCount;

        m_bWorldBoundsEnabled = lSettings.WorldBounds.bEnabled;
        m_WorldBoundsMin      = lSettings.WorldBounds.Min;
        m_WorldBoundsMax      = lSettings.WorldBounds.Max;
        m_WorldBoundsResponse = lSettings.WorldBounds.Response;

        m_World = PhysicsAPI::Create(lSettings.Backend, lDesc);

        if (m_World == nullptr)
        {
            OPAAX_LOG(LogPhysics, Error, "No physics world — the '{}' backend refused to build one.",
                      ToString(lSettings.Backend));
            return false;
        }

        // No entities yet, by contract (WS7) — the first FixedUpdate is what populates the world.
        OPAAX_LOG(LogPhysics, Info, "Physics started (Play world, {} backend, gravity {},{})",
                  ToString(lSettings.Backend), lSettings.Gravity.x, lSettings.Gravity.y);

        if (m_bWorldBoundsEnabled)
        {
            OPAAX_LOG(LogPhysics, Info, "World bounds ON: [{},{}]..[{},{}], response {}",
                      m_WorldBoundsMin.x, m_WorldBoundsMin.y, m_WorldBoundsMax.x, m_WorldBoundsMax.y,
                      ToString(m_WorldBoundsResponse));
        }

        return true;
    }

    void PhysicsSubsystem::Shutdown()
    {
        // The bodies die with the world; the map must not outlive them as stale handles.
        m_Bodies.clear();
        m_World.reset();

        OPAAX_LOG(LogPhysics, Info, "Physics shut down");
    }

    // =========================================================================
    // Tick
    // =========================================================================
    void PhysicsSubsystem::FixedUpdate(const double InFixedDeltaTime)
    {
        if (m_World == nullptr)
        {
            return;
        }

        OPAAX_STAT_SCOPE(m_Context->Profiler, "Physics");

        World& lWorld = m_Context->OwningWorld;

        // Dead first: a body whose entity is gone must not emit contacts for this step.
        ReconcileDeadBodies(lWorld);
        ReconcileLiveBodies(lWorld);

        m_World->Step(static_cast<float>(InFixedDeltaTime), m_SubStepCount);

        SyncDynamicTransforms(lWorld);
        DispatchPhysicsEvents(lWorld);

        // LAST, so every contact and overlap this step produced has already been delivered before
        // anything is reaped — a body that touches something on the way out still reports it.
        EnforceWorldBounds(lWorld);

        // The SUCCESS branch, once ([[L15]]). A subsystem that logged only failures would read
        // identically whether it simulated forty bodies or none at all.
        if (!m_bLoggedFirstStep)
        {
            m_bLoggedFirstStep = true;
            OPAAX_LOG(LogPhysics, Info, "Simulating {} body/bodies", GetBodyCount());
        }
    }

    // =========================================================================
    // Body reconciliation
    // =========================================================================
    void PhysicsSubsystem::ReconcileDeadBodies(World& InWorld)
    {
        m_DeadBodyVictims.clear();

        for (const auto& [lBits, lRecord] : m_Bodies)
        {
            if (!InWorld.IsValid(static_cast<EntityID>(lBits)))
            {
                m_DeadBodyVictims.push_back(lBits);
            }
        }

        // Collected during the walk, destroyed after it — never mutate m_Bodies mid-iteration.
        for (const Uint32 lBits : m_DeadBodyVictims)
        {
            RemoveBodyForEntity(static_cast<EntityID>(lBits));
        }
    }

    void PhysicsSubsystem::ReconcileLiveBodies(World& InWorld)
    {
        InWorld.Each<ColliderComponent, TransformComponent>(
            [this, &InWorld](EntityID InEntity, ColliderComponent& InCollider, TransformComponent& InTransform)
            {
                const auto* lRigidbody = InWorld.GetRegistry().try_get<RigidbodyComponent>(InEntity);
                const EBodyType lWanted = ResolveBodyType(lRigidbody);

                const auto lFound = m_Bodies.find(EntityBits(InEntity));
                if (lFound != m_Bodies.end())
                {
                    // Already built as the right kind: nothing to do. A MISMATCH means the entity
                    // gained or lost a Rigidbody after the body existed, so rebuild — which is what
                    // makes the order components were added stop mattering.
                    if (lFound->second.BuiltType == lWanted)
                    {
                        return;
                    }

                    RemoveBodyForEntity(InEntity);
                }

                BuildBodyForEntity(InWorld, InEntity, InCollider, InTransform);
            });
    }

    void PhysicsSubsystem::BuildBodyForEntity(World& InWorld, const EntityID InEntity,
                                              const ColliderComponent& InCollider,
                                              const TransformComponent& InTransform)
    {
        const auto* lRigidbody = InWorld.GetRegistry().try_get<RigidbodyComponent>(InEntity);

        BodyDesc lBody;
        lBody.Type     = ResolveBodyType(lRigidbody);
        lBody.Position = InTransform.Position;
        lBody.Rotation = Maths::DegreesToRadians(InTransform.Rotation);
        lBody.UserData = ToUserData(InEntity);

        if (lRigidbody != nullptr)
        {
            lBody.GravityScale   = lRigidbody->GravityScale;
            lBody.bFixedRotation = lRigidbody->bFixedRotation;
            lBody.LinearDamping  = lRigidbody->LinearDamping;
            lBody.AngularDamping = lRigidbody->AngularDamping;
        }

        const BodyHandle lHandle = m_World->CreateBody(lBody);
        if (!lHandle.IsValid())
        {
            OPAAX_LOG(LogPhysics, Warn, "Body creation failed for entity {}", EntityBits(InEntity));
            return;
        }

        m_World->AddShape(lHandle, MakeShapeDesc(InCollider));

        BodyRecord lRecord;
        lRecord.Handle           = lHandle;
        lRecord.BuiltType        = lBody.Type;
        lRecord.bSyncToTransform = lBody.Type == EBodyType::Dynamic;

        m_Bodies.emplace(EntityBits(InEntity), lRecord);
        ++m_LastBuiltCount;

        // The first DYNAMIC body becomes the motion probe: a static one can never move, so
        // watching it would prove nothing either way.
        if (m_ProbeEntity == ENTITY_NONE && lRecord.bSyncToTransform)
        {
            m_ProbeEntity = InEntity;
            m_ProbeOrigin = lBody.Position;
        }
    }

    void PhysicsSubsystem::RemoveBodyForEntity(const EntityID InEntity)
    {
        const auto lFound = m_Bodies.find(EntityBits(InEntity));
        if (lFound == m_Bodies.end())
        {
            return;
        }

        m_World->DestroyBody(lFound->second.Handle);
        m_Bodies.erase(lFound);

        // Scrub the live overlaps this body was in, or a pair that can no longer end would sit
        // there being Stayed forever — a body with no shape reports nothing, including its Ended.
        m_StaleOverlaps.clear();
        for (const auto& [lKey, lPair] : m_LiveOverlaps)
        {
            if (FromUserData(lPair.EntityA) == InEntity || FromUserData(lPair.EntityB) == InEntity)
            {
                m_StaleOverlaps.push_back(lKey);
            }
        }

        for (const Uint64 lKey : m_StaleOverlaps)
        {
            m_LiveOverlaps.erase(lKey);
        }
    }

    // =========================================================================
    // Transform sync
    // =========================================================================
    void PhysicsSubsystem::SyncDynamicTransforms(World& InWorld)
    {
        for (const auto& [lBits, lRecord] : m_Bodies)
        {
            // Static and kinematic bodies are DRIVEN, not read: writing their pose back would
            // fight whatever authored or animated it.
            if (!lRecord.bSyncToTransform)
            {
                continue;
            }

            const auto lEntity = static_cast<EntityID>(lBits);
            auto*      lTransform = InWorld.GetRegistry().try_get<TransformComponent>(lEntity);
            if (lTransform == nullptr)
            {
                continue;
            }

            Vector2F lPosition;
            float    lRotation = 0.f;
            m_World->GetBodyTransform(lRecord.Handle, lPosition, lRotation);

            lTransform->Position = lPosition;
            lTransform->Rotation = Maths::RadiansToDegrees(lRotation);

            NoteBodyMoved(lEntity, lPosition);
        }
    }

    void PhysicsSubsystem::NoteBodyMoved(const EntityID InEntity, const Vector2F& InPosition)
    {
        if (m_bLoggedMotion || InEntity != m_ProbeEntity)
        {
            return;
        }

        // A whole world unit, so float noise on a body resting at its authored pose cannot pass
        // for motion — the instrument must not be able to succeed by accident ([[L21]]).
        const Vector2F lDelta    = InPosition - m_ProbeOrigin;
        const float    lDistance = Maths::Sqrt(lDelta.x * lDelta.x + lDelta.y * lDelta.y);

        if (lDistance < 1.f)
        {
            return;
        }

        m_bLoggedMotion = true;
        OPAAX_LOG(LogPhysics, Info, "Body {} has moved {:.1f} units from where it was built — the solver is live",
                  EntityBits(InEntity), lDistance);
    }

    // =========================================================================
    // Events
    // =========================================================================
    Uint64 PhysicsSubsystem::PairKey(const Uint64 InEntityBitsA, const Uint64 InEntityBitsB) noexcept
    {
        const Uint64 lLow  = InEntityBitsA < InEntityBitsB ? InEntityBitsA : InEntityBitsB;
        const Uint64 lHigh = InEntityBitsA < InEntityBitsB ? InEntityBitsB : InEntityBitsA;
        return (lLow << 32) | (lHigh & 0xFFFFFFFFull);
    }

    void PhysicsSubsystem::DispatchPhysicsEvents(World& InWorld)
    {
        EventBus& lBus = m_Context->Events.GetEventBus();

        m_World->GetSensorEvents(m_SensorBegan, m_SensorEnded);
        m_World->GetContactEvents(m_ContactBegan, m_ContactEnded);

        // ---- overlap: Began -------------------------------------------------------------
        for (const PhysicsContactPair& lPair : m_SensorBegan)
        {
            const EntityID lSensor  = FromUserData(lPair.EntityA);
            const EntityID lVisitor = FromUserData(lPair.EntityB);

            if (lSensor == ENTITY_NONE || lVisitor == ENTITY_NONE)
            {
                continue;
            }

            m_LiveOverlaps[PairKey(lPair.EntityA, lPair.EntityB)] = lPair;
            ++m_OverlapEventCount;

            lBus.Publish(PhysicsOverlapBegan{ lSensor, lVisitor });
        }

        // ---- overlap: Ended, BEFORE the survivors tick -----------------------------------
        for (const PhysicsContactPair& lPair : m_SensorEnded)
        {
            m_LiveOverlaps.erase(PairKey(lPair.EntityA, lPair.EntityB));

            const EntityID lSensor  = FromUserData(lPair.EntityA);
            const EntityID lVisitor = FromUserData(lPair.EntityB);

            if (lSensor == ENTITY_NONE || lVisitor == ENTITY_NONE)
            {
                continue;
            }

            ++m_OverlapEventCount;
            lBus.Publish(PhysicsOverlapEnded{ lSensor, lVisitor });
        }

        // ---- overlap: Stayed, for whatever survived both edges ---------------------------
        // A handler above may have destroyed an entity, so each survivor is re-validated here
        // rather than trusted: a dead sensor must not keep reporting.
        m_StaleOverlaps.clear();

        for (const auto& [lKey, lPair] : m_LiveOverlaps)
        {
            const EntityID lSensor  = FromUserData(lPair.EntityA);
            const EntityID lVisitor = FromUserData(lPair.EntityB);

            if (!InWorld.IsValid(lSensor) || !InWorld.IsValid(lVisitor))
            {
                m_StaleOverlaps.push_back(lKey);
                continue;
            }

            ++m_OverlapEventCount;
            lBus.Publish(PhysicsOverlapStayed{ lSensor, lVisitor });
        }

        for (const Uint64 lKey : m_StaleOverlaps)
        {
            m_LiveOverlaps.erase(lKey);
        }

        // ---- solid contacts: edges only, no state to keep ---------------------------------
        for (const PhysicsContactPair& lPair : m_ContactBegan)
        {
            const EntityID lA = FromUserData(lPair.EntityA);
            const EntityID lB = FromUserData(lPair.EntityB);

            if (lA == ENTITY_NONE || lB == ENTITY_NONE)
            {
                continue;
            }

            ++m_CollisionEventCount;
            lBus.Publish(PhysicsCollisionBegan{ lA, lB });
        }

        for (const PhysicsContactPair& lPair : m_ContactEnded)
        {
            const EntityID lA = FromUserData(lPair.EntityA);
            const EntityID lB = FromUserData(lPair.EntityB);

            if (lA == ENTITY_NONE || lB == ENTITY_NONE)
            {
                continue;
            }

            ++m_CollisionEventCount;
            lBus.Publish(PhysicsCollisionEnded{ lA, lB });
        }

        // The success branch, once ([[L15]]) — and it counts, because "physics is running" and
        // "physics is reporting touches" are different claims and only the second one is this.
        if (!m_bLoggedFirstTouch && (m_OverlapEventCount > 0 || m_CollisionEventCount > 0))
        {
            m_bLoggedFirstTouch = true;
            OPAAX_LOG(LogPhysics, Info, "First touches dispatched — {} overlap, {} collision",
                      m_OverlapEventCount, m_CollisionEventCount);
        }
    }

    // =========================================================================
    // Queries
    // =========================================================================
    PhysicsSubsystem::RaycastHit PhysicsSubsystem::RayCast(const Vector2F InOrigin, const Vector2F InDirection,
                                                           const float InDistance, const Uint64 InChannelMask)
    {
        RaycastHit lResult;

        // Nothing is playing, so nothing can be hit. A state, not an error — asking before Play is
        // a legitimate thing for a tool or a script to do.
        if (m_World == nullptr)
        {
            return lResult;
        }

        const PhysicsRayHit lHit = m_World->RayCastClosest(InOrigin, InDirection, InDistance, InChannelMask);
        if (!lHit.bHit)
        {
            return lResult;
        }

        const EntityID lEntity = FromUserData(lHit.UserData);

        // A hit whose body carries no resolvable entity is reported as a MISS rather than as a hit
        // on ENTITY_NONE: every caller would have to check, and most would forget.
        if (lEntity == ENTITY_NONE)
        {
            return lResult;
        }

        lResult.bHit     = true;
        lResult.Entity   = lEntity;
        lResult.Point    = lHit.Point;
        lResult.Normal   = lHit.Normal;
        lResult.Fraction = lHit.Fraction;
        return lResult;
    }

    void PhysicsSubsystem::OverlapAABB(const Vector2F InMin, const Vector2F InMax,
                                       TDynArray<EntityID>& OutEntities, const Uint64 InChannelMask)
    {
        OutEntities.clear();

        if (m_World == nullptr)
        {
            return;
        }

        m_World->OverlapAABB(InMin, InMax, InChannelMask, m_QueryScratch);

        for (const Uint64 lUserData : m_QueryScratch)
        {
            const EntityID lEntity = FromUserData(lUserData);
            if (lEntity != ENTITY_NONE)
            {
                OutEntities.push_back(lEntity);
            }
        }
    }

    // =========================================================================
    // World bounds
    // =========================================================================
    void PhysicsSubsystem::EnforceWorldBounds(World& InWorld)
    {
        if (!m_bWorldBoundsEnabled)
        {
            return;
        }

        EventBus& lBus = m_Context->Events.GetEventBus();

        m_BoundsVictims.clear();

        for (const auto& [lBits, lRecord] : m_Bodies)
        {
            // Only bodies that MOVE on their own can leave: a static collider outside the bounds
            // was authored there, and reaping it would delete level geometry.
            if (!lRecord.bSyncToTransform)
            {
                continue;
            }

            Vector2F lPosition;
            float    lRotation = 0.f;
            m_World->GetBodyTransform(lRecord.Handle, lPosition, lRotation);

            const bool bInside = lPosition.x >= m_WorldBoundsMin.x && lPosition.x <= m_WorldBoundsMax.x
                              && lPosition.y >= m_WorldBoundsMin.y && lPosition.y <= m_WorldBoundsMax.y;

            if (bInside)
            {
                // Back inside: un-latch, so leaving again reports again.
                m_OutOfBounds.erase(lBits);
                continue;
            }

            // Already reported. A body that keeps falling is one occurrence, not sixty a second.
            if (m_OutOfBounds.find(lBits) != m_OutOfBounds.end())
            {
                continue;
            }

            m_OutOfBounds.insert(lBits);

            // Published BEFORE the reap, so a handler still sees a live entity.
            lBus.Publish(PhysicsExitedWorldBounds{ static_cast<EntityID>(lBits), lPosition });

            // Once, and only for the first ([[L15]]): "World bounds ON" says the feature is
            // configured, which is a different claim from anything ever having left them. Not per
            // exit — a level draining into a pit would print a line per body.
            if (!m_bLoggedFirstExit)
            {
                m_bLoggedFirstExit = true;
                OPAAX_LOG(LogPhysics, Info, "Entity {} left the world bounds at ({:.0f},{:.0f}) — {}",
                          lBits, lPosition.x, lPosition.y, ToString(m_WorldBoundsResponse));
            }

            if (m_WorldBoundsResponse == EWorldBoundsResponse::EventAndDestroy)
            {
                m_BoundsVictims.push_back(lBits);
            }
        }

        // Collected during the walk, destroyed after it — never mutate m_Bodies mid-iteration.
        for (const Uint32 lBits : m_BoundsVictims)
        {
            const auto lEntity = static_cast<EntityID>(lBits);

            // A handler may already have destroyed it, which is a supported thing to do.
            if (InWorld.IsValid(lEntity))
            {
                InWorld.DestroyEntity(lEntity);
            }

            RemoveBodyForEntity(lEntity);
            m_OutOfBounds.erase(lBits);
        }
    }

    // =========================================================================
    // Translation
    // =========================================================================
    EBodyType PhysicsSubsystem::ResolveBodyType(const RigidbodyComponent* InRigidbody) noexcept
    {
        // No rigidbody means STATIC: level geometry is the common case and should not need a
        // second component to say what it already is.
        return InRigidbody != nullptr ? InRigidbody->Type : EBodyType::Static;
    }

    ShapeDesc PhysicsSubsystem::MakeShapeDesc(const ColliderComponent& InCollider)
    {
        ShapeDesc lShape;
        lShape.Geometry.Type   = InCollider.Shape;
        lShape.Geometry.Offset = InCollider.Offset;

        // The component authors FULL size, the seam takes half extents. Halved here, once, so no
        // caller has to remember which convention it is holding.
        lShape.Geometry.HalfExtents = InCollider.Size * 0.5f;
        lShape.Geometry.Radius      = InCollider.Radius;

        if (InCollider.Shape == EColliderShape::Capsule)
        {
            // The caps sit a radius in from each end, so a capsule of Size.y is exactly that tall.
            const float lHalfSpan = Maths::Max(0.f, InCollider.Size.y * 0.5f - InCollider.Radius);
            lShape.Geometry.Center1 = { 0.f, -lHalfSpan };
            lShape.Geometry.Center2 = { 0.f,  lHalfSpan };
        }

        lShape.bIsSensor   = InCollider.Mode == EColliderMode::Overlap;
        lShape.Density     = InCollider.Density;
        lShape.Friction    = InCollider.Friction;
        lShape.Restitution = InCollider.Restitution;

        // The channel IS the category bit. What it collides WITH is everything, until the
        // CollisionProfile resource lands and fills the mask (PH4).
        lShape.CategoryBits = CategoryBit(InCollider.Channel);
        lShape.MaskBits     = AllChannelsMask();

        return lShape;
    }
}
