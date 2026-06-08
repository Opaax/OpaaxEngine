#include "PhysicsSubsystem.h"

#include "Core/Config/EngineConfig.h"
#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"

#include "Physics/PhysicsAPI.h"
#include "Physics/Collision/CollisionChannel.h"
#include "Physics/Collision/CollisionProfile.h"
#include "Physics/Events/PhysicsEvents.h"

#include "World/World.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ColliderComponent.h"

namespace Opaax
{
    namespace
    {
        // Order-independent key so an overlapping (A,B) and (B,A) collapse to one live entry.
        Uint64 OverlapKey(Uint64 InA, Uint64 InB) noexcept
        {
            const Uint32 lA  = static_cast<Uint32>(InA);
            const Uint32 lB  = static_cast<Uint32>(InB);
            const Uint32 lLo = lA < lB ? lA : lB;
            const Uint32 lHi = lA < lB ? lB : lA;
            return (static_cast<Uint64>(lHi) << 32) | lLo;
        }

        // Body user-data is entity-bits + 1 (see BuildBodies); 0 means no entity / stale shape.
        bool TryDecodeEntity(Uint64 InUserData, EntityID& OutEntity) noexcept
        {
            if (InUserData == 0) { return false; }
            OutEntity = static_cast<EntityID>(static_cast<Uint32>(InUserData - 1));
            return true;
        }
    }

    bool PhysicsSubsystem::Startup()
    {
        const EPhysicsBackend lBackend = PhysicsAPI::BackendFromString(EngineConfig::PhysicsBackend());
        m_World = PhysicsAPI::Create(lBackend, m_WorldDesc);

        if (m_World == nullptr)
        {
            OPAAX_CORE_ERROR("PhysicsSubsystem::Startup — failed to create physics world (backend '{}').",
                             PhysicsAPI::BackendToString(lBackend));
            return false;
        }

        OPAAX_CORE_INFO("PhysicsSubsystem::Startup — backend '{}'.", PhysicsAPI::BackendToString(lBackend));
        return true;
    }

    void PhysicsSubsystem::FixedUpdate(double FixedDeltaTime)
    {
        if (m_World == nullptr) { return; }

        m_World->Step(static_cast<float>(FixedDeltaTime), m_WorldDesc.SubStepCount);

        if (CoreEngineApp* lApp = GetEngineApp())
        {
            SyncDynamicTransforms(lApp->GetWorld());
        }

        // Fan out overlap/collision events after the step + sync (never mid body-iteration).
        DispatchPhysicsEvents();
    }

    void PhysicsSubsystem::Shutdown()
    {
        ClearBodies();
        m_World.reset();
        OPAAX_CORE_INFO("PhysicsSubsystem::Shutdown");
    }

    // =============================================================================
    // Play-session edges
    // =============================================================================
    void PhysicsSubsystem::OnPlayBegin()
    {
        if (m_World == nullptr) { return; }

        CoreEngineApp* lApp = GetEngineApp();
        if (lApp == nullptr) { return; }

        BuildBodies(lApp->GetWorld());
        OPAAX_CORE_INFO("PhysicsSubsystem::OnPlayBegin — built {} bodies.", m_Bodies.size());
    }

    void PhysicsSubsystem::OnPlayEnd()
    {
        const size_t lCount = m_Bodies.size();
        ClearBodies();
        OPAAX_CORE_INFO("PhysicsSubsystem::OnPlayEnd — destroyed {} bodies.", lCount);
    }

    // =============================================================================
    // Body build / teardown / sync
    // =============================================================================
    void PhysicsSubsystem::BuildBodies(World& InWorld)
    {
        // Belt-and-braces — never double-build into a populated map.
        ClearBodies();

        using namespace ECS;
        InWorld.Each<ColliderComponent, TransformComponent>(
            [this, &InWorld](EntityID InEntity, ColliderComponent& InCollider, TransformComponent& InTransform)
            {
                // Rigidbody is optional — a lone collider becomes an implicit static body.
                const RigidbodyComponent* lRb = InWorld.GetComponent<RigidbodyComponent>(InEntity);

                BodyDesc lBodyDesc;
                lBodyDesc.Type           = lRb ? lRb->Type           : EBodyType::Static;
                lBodyDesc.Position       = InTransform.Position;
                lBodyDesc.Rotation       = InTransform.Rotation;
                lBodyDesc.GravityScale   = lRb ? lRb->GravityScale   : 1.f;
                lBodyDesc.FixedRotation  = lRb ? lRb->FixedRotation  : false;
                lBodyDesc.LinearDamping  = lRb ? lRb->LinearDamping  : 0.f;
                lBodyDesc.AngularDamping = lRb ? lRb->AngularDamping : 0.f;
                // Encode as entity-bits + 1 so 0 stays reserved for "no entity / stale shape"
                // (entt's first entity has raw value 0, which a sensor event must not alias).
                lBodyDesc.UserData       = static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1;

                const BodyHandle lBody = m_World->CreateBody(lBodyDesc);
                if (!lBody.IsValid()) { return; }

                ShapeDesc lShapeDesc;
                lShapeDesc.Geometry.Type        = InCollider.Shape;
                lShapeDesc.Geometry.Offset      = InCollider.Offset;
                // Component authors full extents; geometry stores half-extents.
                lShapeDesc.Geometry.HalfExtents = { InCollider.Size.x * 0.5f, InCollider.Size.y * 0.5f };
                lShapeDesc.Geometry.Radius      = InCollider.Radius;
                lShapeDesc.IsSensor     = (InCollider.Mode == EColliderMode::Overlap);
                lShapeDesc.Density      = InCollider.Density;
                lShapeDesc.Friction     = InCollider.Friction;
                lShapeDesc.Restitution  = InCollider.Restitution;

                // Profile (when assigned + loaded) drives both the category bit (its channel)
                // and the mask (its response matrix: Ignore clears, Overlap/Block set). Without
                // a profile the collider uses its own Channel and collides with everything.
                if (const CollisionProfile* lProfile = InCollider.Profile.Get())
                {
                    lShapeDesc.CategoryBits = lProfile->ComputeCategoryBits();
                    lShapeDesc.MaskBits     = lProfile->ComputeMaskBits();
                }
                else
                {
                    lShapeDesc.CategoryBits = CategoryBit(InCollider.Channel);
                    lShapeDesc.MaskBits     = ~0ull;
                }

                m_World->AddShape(lBody, lShapeDesc);

                BodyRecord lRecord;
                lRecord.Handle           = lBody;
                lRecord.bSyncToTransform = (lBodyDesc.Type == EBodyType::Dynamic);
                m_Bodies.emplace(static_cast<Uint32>(InEntity), lRecord);
            });
    }

    void PhysicsSubsystem::ClearBodies()
    {
        if (m_World != nullptr)
        {
            for (auto& [lBits, lRecord] : m_Bodies)
            {
                m_World->DestroyBody(lRecord.Handle);
            }
        }
        m_Bodies.clear();

        // Drop tracked overlaps so no stale pair survives into the next play session.
        m_LiveOverlaps.clear();
    }

    void PhysicsSubsystem::SyncDynamicTransforms(World& InWorld)
    {
        for (auto& [lBits, lRecord] : m_Bodies)
        {
            if (!lRecord.bSyncToTransform) { continue; }

            const EntityID lEntity = static_cast<EntityID>(lBits);
            ECS::TransformComponent* lTransform = InWorld.GetComponent<ECS::TransformComponent>(lEntity);
            if (lTransform == nullptr) { continue; }

            Vector2F lPosition;
            float    lRotation = 0.f;
            m_World->GetBodyTransform(lRecord.Handle, lPosition, lRotation);

            lTransform->Position = lPosition;
            lTransform->Rotation = lRotation;
        }
    }

    // =============================================================================
    // Event dispatch
    // =============================================================================
    void PhysicsSubsystem::DispatchPhysicsEvents()
    {
        if (m_World == nullptr) { return; }

        CoreEngineApp* lApp = GetEngineApp();
        if (lApp == nullptr) { return; }

        // --- Overlap (sensor): Start then Stop, then synthesize Tick for the survivors ---
        m_World->GetSensorEvents(m_SensorBegan, m_SensorEnded);

        for (const PhysicsContactPair& lPair : m_SensorBegan)
        {
            m_LiveOverlaps[OverlapKey(lPair.EntityA, lPair.EntityB)] = lPair;

            EntityID lOverlap, lOther;
            if (TryDecodeEntity(lPair.EntityA, lOverlap) && TryDecodeEntity(lPair.EntityB, lOther))
            {
                OnOverlapStartEvent lEvent(lOverlap, lOther);
                lApp->DispatchEvent(lEvent);
            }
        }

        for (const PhysicsContactPair& lPair : m_SensorEnded)
        {
            m_LiveOverlaps.erase(OverlapKey(lPair.EntityA, lPair.EntityB));

            EntityID lOverlap, lOther;
            if (TryDecodeEntity(lPair.EntityA, lOverlap) && TryDecodeEntity(lPair.EntityB, lOther))
            {
                OnOverlapStopEvent lEvent(lOverlap, lOther);
                lApp->DispatchEvent(lEvent);
            }
        }

        // A pair that began AND ended this step is already gone from the map -> Start+Stop, no Tick.
        for (const auto& [lKey, lPair] : m_LiveOverlaps)
        {
            EntityID lOverlap, lOther;
            if (TryDecodeEntity(lPair.EntityA, lOverlap) && TryDecodeEntity(lPair.EntityB, lOther))
            {
                OnOverlapTickEvent lEvent(lOverlap, lOther);
                lApp->DispatchEvent(lEvent);
            }
        }

        // --- Collision (solid): Enter / Exit only, no tracking ---
        m_World->GetContactEvents(m_ContactBegan, m_ContactEnded);

        for (const PhysicsContactPair& lPair : m_ContactBegan)
        {
            EntityID lA, lB;
            if (TryDecodeEntity(lPair.EntityA, lA) && TryDecodeEntity(lPair.EntityB, lB))
            {
                OnCollisionEnterEvent lEvent(lA, lB);
                lApp->DispatchEvent(lEvent);
            }
        }

        for (const PhysicsContactPair& lPair : m_ContactEnded)
        {
            EntityID lA, lB;
            if (TryDecodeEntity(lPair.EntityA, lA) && TryDecodeEntity(lPair.EntityB, lB))
            {
                OnCollisionExitEvent lEvent(lA, lB);
                lApp->DispatchEvent(lEvent);
            }
        }
    }
}
