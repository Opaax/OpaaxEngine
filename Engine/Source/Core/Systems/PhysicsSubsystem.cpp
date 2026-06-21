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
#include "ECS/Components/TransformInterpolationComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/MoverComponent.h"

#include "Core/Systems/Movement/MoverModeRegistry.h"
#include "Core/Systems/Movement/IMoverMode.h"
#include "Core/Systems/Movement/GroundMoveMode.h"
#include "Core/Systems/Movement/FlyMoveMode.h"

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

        // World-bounds kill volume — config-driven, off + generous by default.
        m_WorldBoundsEnabled  = EngineConfig::PhysicsWorldBoundsEnabled();
        m_WorldBoundsMin      = EngineConfig::PhysicsWorldBoundsMin();
        m_WorldBoundsMax      = EngineConfig::PhysicsWorldBoundsMax();
        m_WorldBoundsResponse = WorldBoundsResponseFromString(EngineConfig::PhysicsWorldBoundsResponse());

        OPAAX_CORE_INFO("PhysicsSubsystem::Startup — backend '{}', worldBounds {} (min [{}, {}] max [{}, {}] response '{}').",
                        PhysicsAPI::BackendToString(lBackend),
                        m_WorldBoundsEnabled ? "enabled" : "disabled",
                        m_WorldBoundsMin.x, m_WorldBoundsMin.y, m_WorldBoundsMax.x, m_WorldBoundsMax.y,
                        ToString(m_WorldBoundsResponse));

        // Built-in mover modes (folded in from MoverSubsystem). Game/engine code adds more via
        // MoverModeRegistry::Register; new movement = a new mode, never a change here.
        MoverModeRegistry::Register(OPAAX_ID("GroundMove"), MakeUnique<GroundMoveMode>());
        MoverModeRegistry::Register(OPAAX_ID("Fly"),        MakeUnique<FlyMoveMode>());

        return true;
    }

    void PhysicsSubsystem::FixedUpdate(double FixedDeltaTime)
    {
        if (m_World == nullptr) { return; }

        // Keep the body set in sync with the live collider-entity set BEFORE stepping: drop bodies
        // whose entity was destroyed (gameplay handler, script, ...) so a dead body can't emit stale
        // events, and build bodies for entities spawned / given a collider at runtime so they join
        // the sim this step.
        if (CoreEngineApp* lApp = GetEngineApp())
        {
            World& lWorld = lApp->GetWorld();
            ReconcileDeadBodies(lWorld);
            ReconcileLiveBodies(lWorld);
            // Movers (folded in): set each mover's kinematic target BEFORE the step so the solver applies
            // it this same step (was a separate MoverSubsystem registered before PhysicsSubsystem).
            TickMovers(lWorld, FixedDeltaTime);
        }

        m_World->Step(static_cast<float>(FixedDeltaTime), m_WorldDesc.SubStepCount);

        if (CoreEngineApp* lApp = GetEngineApp())
        {
            SyncDynamicTransforms(lApp->GetWorld());
        }

        // Fan out overlap/collision events after the step + sync (never mid body-iteration).
        DispatchPhysicsEvents();

        // Reap out-of-bounds bodies LAST — this step's contact/overlap events have already fired.
        if (m_WorldBoundsEnabled)
        {
            if (CoreEngineApp* lApp = GetEngineApp())
            {
                EnforceWorldBounds(lApp->GetWorld());
            }
        }
    }

    void PhysicsSubsystem::Shutdown()
    {
        ClearMoverBodies();          // mover kinematic bodies — destroy while m_World is still alive
        ClearBodies();               // collider bodies
        MoverModeRegistry::Clear();  // drop the mover-mode catalog (folded in from MoverSubsystem)
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

        // Movers (folded in): reconcile per-mode params (covers code-created movers that skipped
        // AddMode/load) then build their kinematic bodies.
        lApp->GetWorld().Each<ECS::MoverComponent>(
            [](EntityID /*InEntity*/, ECS::MoverComponent& InMover) { InMover.EnsureModeParams(); });
        BuildMoverBodies(lApp->GetWorld());
        OPAAX_CORE_INFO("PhysicsSubsystem::OnPlayBegin — built {} mover bodies.", m_MoverBodies.size());
    }

    void PhysicsSubsystem::OnPlayEnd()
    {
        const size_t lCount      = m_Bodies.size();
        const size_t lMoverCount = m_MoverBodies.size();
        ClearBodies();
        ClearMoverBodies();
        OPAAX_CORE_INFO("PhysicsSubsystem::OnPlayEnd — destroyed {} bodies, {} mover bodies.", lCount, lMoverCount);
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
            [this, &InWorld](EntityID InEntity, ColliderComponent& /*InCollider*/, TransformComponent& /*InTransform*/)
            {
                BuildBodyForEntity(InWorld, InEntity);
            });
    }

    void PhysicsSubsystem::BuildBodyForEntity(World& InWorld, EntityID InEntity)
    {
        if (m_World == nullptr) { return; }

        using namespace ECS;
        ColliderComponent*  lCollider  = InWorld.GetComponent<ColliderComponent>(InEntity);
        TransformComponent* lTransform = InWorld.GetComponent<TransformComponent>(InEntity);
        if (lCollider == nullptr || lTransform == nullptr) { return; }

        // Rigidbody is optional — a lone collider becomes an implicit static body.
        const RigidbodyComponent* lRb = InWorld.GetComponent<RigidbodyComponent>(InEntity);

        BodyDesc lBodyDesc;
        lBodyDesc.Type           = lRb ? lRb->Type           : EBodyType::Static;
        lBodyDesc.Position       = lTransform->Position;
        lBodyDesc.Rotation       = lTransform->Rotation;
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
        lShapeDesc.Geometry.Type        = lCollider->Shape;
        lShapeDesc.Geometry.Offset      = lCollider->Offset;
        // Component authors full extents; geometry stores half-extents.
        lShapeDesc.Geometry.HalfExtents = { lCollider->Size.x * 0.5f, lCollider->Size.y * 0.5f };
        lShapeDesc.Geometry.Radius      = lCollider->Radius;
        lShapeDesc.IsSensor     = (lCollider->Mode == EColliderMode::Overlap);
        lShapeDesc.Density      = lCollider->Density;
        lShapeDesc.Friction     = lCollider->Friction;
        lShapeDesc.Restitution  = lCollider->Restitution;

        // Profile (when assigned + loaded) drives both the category bit (its channel)
        // and the mask (its response matrix: Ignore clears, Overlap/Block set). Without
        // a profile the collider uses its own Channel and collides with everything.
        if (const CollisionProfile* lProfile = lCollider->Profile.Get())
        {
            lShapeDesc.CategoryBits = lProfile->ComputeCategoryBits();
            lShapeDesc.MaskBits     = lProfile->ComputeMaskBits();
        }
        else
        {
            lShapeDesc.CategoryBits = CategoryBit(lCollider->Channel);
            lShapeDesc.MaskBits     = ~0ull;
        }

        m_World->AddShape(lBody, lShapeDesc);

        BodyRecord lRecord;
        lRecord.Handle           = lBody;
        lRecord.bSyncToTransform = (lBodyDesc.Type == EBodyType::Dynamic);
        lRecord.BuiltType        = lBodyDesc.Type;
        m_Bodies.emplace(static_cast<Uint32>(InEntity), lRecord);

        // Only dynamic bodies move on their own and stutter at >60 Hz. Seed the
        // render-interpolation prev pose to the start pose so frame 0 doesn't pop.
        if (lRecord.bSyncToTransform)
        {
            InWorld.AddOrReplaceComponent<TransformInterpolationComponent>(
                InEntity, TransformInterpolationComponent{ lTransform->Position, lTransform->Rotation });
        }
    }

    void PhysicsSubsystem::ReconcileLiveBodies(World& InWorld)
    {
        using namespace ECS;
        InWorld.Each<ColliderComponent, TransformComponent>(
            [this, &InWorld](EntityID InEntity, ColliderComponent& /*InCollider*/, TransformComponent& /*InTransform*/)
            {
                const auto lIt = m_Bodies.find(static_cast<Uint32>(InEntity));
                if (lIt == m_Bodies.end())
                {
                    // No body yet — spawned at runtime, or just gained a Collider.
                    BuildBodyForEntity(InWorld, InEntity);
                    return;
                }

                // Body exists — rebuild if its type no longer matches the current components
                // (e.g. a Rigidbody was added/removed/retyped after the body was first built).
                const RigidbodyComponent* lRb = InWorld.GetComponent<RigidbodyComponent>(InEntity);
                const EBodyType lDesiredType   = lRb ? lRb->Type : EBodyType::Static;
                if (lIt->second.BuiltType != lDesiredType)
                {
                    RemoveBodyForEntity(InEntity);
                    BuildBodyForEntity(InWorld, InEntity);
                }
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

        // Drop tracked overlaps + bounds latch so no stale state survives into the next play session.
        m_LiveOverlaps.clear();
        m_OutOfBounds.clear();
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

            // Snapshot the pre-step pose for render interpolation BEFORE overwriting with
            // this step's result — prev/current then bracket exactly one fixed step.
            if (auto* lInterp = InWorld.GetComponent<ECS::TransformInterpolationComponent>(lEntity))
            {
                lInterp->PrevPosition = lTransform->Position;
                lInterp->PrevRotation = lTransform->Rotation;
            }

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

    // =============================================================================
    // World bounds (kill volume)
    // =============================================================================
    void PhysicsSubsystem::EnforceWorldBounds(World& InWorld)
    {
        if (m_World == nullptr) { return; }

        CoreEngineApp* lApp = GetEngineApp();
        if (lApp == nullptr) { return; }

        m_BoundsVictims.clear();

        for (auto& [lBits, lRecord] : m_Bodies)
        {
            // Only fully-simulated bodies drift on their own; author-placed static/kinematic are skipped.
            if (!lRecord.bSyncToTransform) { continue; }

            Vector2F lPos;
            float    lRot = 0.f;
            m_World->GetBodyTransform(lRecord.Handle, lPos, lRot);

            // Center-point containment — sufficient for a kill volume (the body has drifted far).
            const bool bInside = (lPos.x >= m_WorldBoundsMin.x && lPos.x <= m_WorldBoundsMax.x &&
                                  lPos.y >= m_WorldBoundsMin.y && lPos.y <= m_WorldBoundsMax.y);

            const auto lLatched = m_OutOfBounds.find(lBits);

            if (!bInside && lLatched == m_OutOfBounds.end())
            {
                // Inside -> outside transition: fire once, latch, optionally queue for reaping.
                m_OutOfBounds.insert(lBits);

                const EntityID lEntity = static_cast<EntityID>(lBits);
                OnExitWorldBoundsEvent lEvent(lEntity, lPos);
                lApp->DispatchEvent(lEvent);
                OPAAX_CORE_INFO("PhysicsSubsystem::EnforceWorldBounds — entity {} left world bounds at ({}, {}).",
                                lBits, lPos.x, lPos.y);

                if (m_WorldBoundsResponse == EWorldBoundsResponse::EventAndDestroy)
                {
                    m_BoundsVictims.push_back(lBits);
                }
            }
            else if (bInside && lLatched != m_OutOfBounds.end())
            {
                // Re-entered the bounds — clear the latch so a future exit fires again.
                m_OutOfBounds.erase(lLatched);
            }
        }

        // Reap after the walk — RemoveBodyForEntity mutates m_Bodies/m_OutOfBounds.
        for (const Uint32 lBits : m_BoundsVictims)
        {
            const EntityID lEntity = static_cast<EntityID>(lBits);
            RemoveBodyForEntity(lEntity);
            InWorld.DestroyEntity(lEntity);
        }
    }

    void PhysicsSubsystem::RemoveBodyForEntity(EntityID InEntity)
    {
        const Uint32 lBits = static_cast<Uint32>(InEntity);

        const auto lIt = m_Bodies.find(lBits);
        if (lIt != m_Bodies.end())
        {
            if (m_World != nullptr) { m_World->DestroyBody(lIt->second.Handle); }
            m_Bodies.erase(lIt);
        }

        m_OutOfBounds.erase(lBits);

        // Body user-data is entity bits + 1 (see BuildBodies); scrub any live overlap that
        // referenced this entity so no phantom OnOverlapTick fires after the kill.
        const Uint64 lUserData = static_cast<Uint64>(lBits) + 1;
        for (auto lOv = m_LiveOverlaps.begin(); lOv != m_LiveOverlaps.end();)
        {
            if (lOv->second.EntityA == lUserData || lOv->second.EntityB == lUserData)
            {
                lOv = m_LiveOverlaps.erase(lOv);
            }
            else
            {
                ++lOv;
            }
        }
    }

    void PhysicsSubsystem::ReconcileDeadBodies(World& InWorld)
    {
        if (m_Bodies.empty()) { return; }

        m_DeadBodyVictims.clear();

        // Collect first — RemoveBodyForEntity mutates m_Bodies, so never erase mid-iteration.
        for (auto& [lBits, lRecord] : m_Bodies)
        {
            if (!InWorld.IsValid(static_cast<EntityID>(lBits)))
            {
                m_DeadBodyVictims.push_back(lBits);
            }
        }

        for (const Uint32 lBits : m_DeadBodyVictims)
        {
            RemoveBodyForEntity(static_cast<EntityID>(lBits));
        }
    }

    // =============================================================================
    // Movers (folded in from the former MoverSubsystem)
    // =============================================================================
    void PhysicsSubsystem::BuildMoverBodies(World& InWorld)
    {
        if (m_World == nullptr) { return; }
        ClearMoverBodies();

        using namespace ECS;
        InWorld.Each<MoverComponent, TransformComponent>(
            [this, &InWorld](EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform)
            {
                // A mover is a KINEMATIC body — we drive it; the solver never moves it. Fixed rotation.
                BodyDesc lBodyDesc;
                lBodyDesc.Type          = EBodyType::Kinematic;
                lBodyDesc.Position      = InTransform.Position;
                lBodyDesc.Rotation      = InTransform.Rotation;
                lBodyDesc.FixedRotation = true;
                lBodyDesc.UserData      = static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1;

                const BodyHandle lBody = m_World->CreateBody(lBodyDesc);
                if (!lBody.IsValid()) { return; }

                const MoverCapsule lCapsule = InMover.BuildCapsule();

                ShapeDesc lShapeDesc;
                lShapeDesc.Geometry.Type    = EColliderShape::Capsule;
                lShapeDesc.Geometry.Center1 = lCapsule.Center1;
                lShapeDesc.Geometry.Center2 = lCapsule.Center2;
                lShapeDesc.Geometry.Radius  = lCapsule.Radius;
                lShapeDesc.IsSensor         = false;   // solid presence: contacts + pushes + events
                // Category = the profile's channel, else default to Pawn (movers are usually pawns).
                if (const CollisionProfile* lProfile = InMover.Profile.Get())
                {
                    lShapeDesc.CategoryBits = lProfile->ComputeCategoryBits();
                }
                else
                {
                    lShapeDesc.CategoryBits = CategoryBit(ECollisionChannel::Pawn);
                }
                // Mask = event reach (Block + Overlap). Movement-solid (Block only) is applied in the mode.
                lShapeDesc.MaskBits = InMover.EffectiveEventMask();

                m_World->AddShape(lBody, lShapeDesc);
                m_MoverBodies.emplace(static_cast<Uint32>(InEntity), lBody);

                // Seed render-interpolation prev pose to the start pose (frame 0 must not pop).
                InWorld.AddOrReplaceComponent<TransformInterpolationComponent>(
                    InEntity, TransformInterpolationComponent{ InTransform.Position, InTransform.Rotation });
            });
    }

    void PhysicsSubsystem::ClearMoverBodies()
    {
        if (m_World != nullptr)
        {
            for (auto& [lBits, lBody] : m_MoverBodies)
            {
                m_World->DestroyBody(lBody);
            }
        }
        m_MoverBodies.clear();
    }

    void PhysicsSubsystem::TickMovers(World& InWorld, double FixedDeltaTime)
    {
        if (m_World == nullptr) { return; }

        const float    lDt    = static_cast<float>(FixedDeltaTime);
        IPhysicsWorld* lWorld = m_World.get();

        using namespace ECS;
        InWorld.Each<MoverComponent, TransformComponent>(
            [this, lWorld, lDt, &InWorld](EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform)
            {
                const Uint32 lBits = static_cast<Uint32>(InEntity);

                // Snapshot the pre-tick pose for render interpolation before the mode mutates the
                // transform — prev/current then bracket exactly one fixed step.
                if (auto* lInterp = InWorld.GetComponent<TransformInterpolationComponent>(InEntity))
                {
                    lInterp->PrevPosition = InTransform.Position;
                    lInterp->PrevRotation = InTransform.Rotation;
                }

                MoverTickContext lContext{ *lWorld, InMover, InTransform, lDt };
                lContext.SelfUserData = static_cast<Uint64>(lBits) + 1;   // skip our own body in the sweep

                // --- Apply a queued mode switch, constrained to the mover's supported set ---
                if (InMover.PendingMode.IsValid())
                {
                    const OpaaxStringID lNext     = InMover.PendingMode;
                    const bool          lReenter  = InMover.PendingReenter;
                    InMover.PendingMode    = OpaaxStringID();   // consume the request
                    InMover.PendingReenter = false;

                    if (!InMover.SupportsMode(lNext))
                    {
                        if (m_WarnedUnknownModes.insert(lNext.GetId()).second)
                        {
                            OPAAX_CORE_WARN("PhysicsSubsystem (mover) — mover does not support mode '{}'; switch ignored.", lNext);
                        }
                    }
                    else
                    {
                        const bool lChanging = (lNext != InMover.ModeId);
                        if (lChanging || lReenter)
                        {
                            MoverTickContext lTransitionCtx{ *lWorld, InMover, InTransform, 0.f };

                            if (lChanging)
                            {
                                if (IMoverMode* lOld = MoverModeRegistry::Find(InMover.ModeId))
                                {
                                    lOld->OnModeExit(lTransitionCtx);
                                }
                                InMover.ModeId = lNext;
                            }

                            if (IMoverMode* lNew = MoverModeRegistry::Find(InMover.ModeId))
                            {
                                lNew->OnModeEnter(lTransitionCtx);
                            }
                        }
                    }
                }

                // --- Self-heal: the active mode must be one the mover supports (edited set / old scene) ---
                if (!InMover.SupportsMode(InMover.ModeId))
                {
                    if (InMover.Modes.empty())
                    {
                        if (m_WarnedUnknownModes.insert(InMover.ModeId.GetId()).second)
                        {
                            OPAAX_CORE_WARN("PhysicsSubsystem (mover) — mover has no supported modes; entity skipped.");
                        }
                        return;
                    }
                    InMover.ModeId = InMover.Modes[0];
                }

                // --- Dispatch the active mode ---
                IMoverMode* lMode = MoverModeRegistry::Find(InMover.ModeId);
                if (lMode == nullptr)
                {
                    if (m_WarnedUnknownModes.insert(InMover.ModeId.GetId()).second)
                    {
                        OPAAX_CORE_WARN("PhysicsSubsystem (mover) — no mover mode registered for '{}'; entity skipped.",
                                        InMover.ModeId);
                    }
                    return;
                }

                // Hand the mode its own params (it downcasts). Reconciled at play-begin, so non-null.
                lContext.Params = InMover.GetModeParams(InMover.ModeId);
                if (lContext.Params == nullptr)
                {
                    if (m_WarnedUnknownModes.insert(InMover.ModeId.GetId()).second)
                    {
                        OPAAX_CORE_WARN("PhysicsSubsystem (mover) — no params for mode '{}'; entity skipped.", InMover.ModeId);
                    }
                    return;
                }

                lMode->Tick(lContext);

                // Drive the kinematic body to the mode's solved pose: it moves there during the Step —
                // colliding, pushing dynamics, and firing contact/sensor events resolved back by user-data.
                const auto lBodyIt = m_MoverBodies.find(lBits);
                if (lBodyIt != m_MoverBodies.end())
                {
                    lWorld->SetBodyTargetTransform(lBodyIt->second, InTransform.Position,
                                                   InTransform.Rotation, lDt);
                }
            });
    }

    // =============================================================================
    // Queries
    // =============================================================================
    PhysicsSubsystem::RaycastResult PhysicsSubsystem::RayCast(Vector2F Origin, Vector2F Direction,
                                                             float Distance, Uint64 ChannelMask)
    {
        RaycastResult lResult;
        if (m_World == nullptr) { return lResult; }

        const PhysicsRayHit lHit = m_World->RayCastClosest(Origin, Direction, Distance, ChannelMask);
        lResult.bHit     = lHit.bHit;
        lResult.Point    = lHit.Point;
        lResult.Normal   = lHit.Normal;
        lResult.Fraction = lHit.Fraction;
        if (lHit.bHit) { TryDecodeEntity(lHit.UserData, lResult.Entity); }
        return lResult;
    }

    void PhysicsSubsystem::OverlapAABB(Vector2F Min, Vector2F Max, TDynArray<EntityID>& Out, Uint64 ChannelMask)
    {
        Out.clear();
        if (m_World == nullptr) { return; }

        TDynArray<Uint64> lBits;
        m_World->OverlapAABB(Min, Max, ChannelMask, lBits);

        Out.reserve(lBits.size());
        for (const Uint64 lUserData : lBits)
        {
            EntityID lEntity;
            if (TryDecodeEntity(lUserData, lEntity)) { Out.push_back(lEntity); }
        }
    }
}
