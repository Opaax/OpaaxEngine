#include "MoverSubsystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"

#include "Core/Systems/PhysicsSubsystem.h"
#include "Core/Systems/Movement/MoverModeRegistry.h"
#include "Core/Systems/Movement/IMoverMode.h"
#include "Core/Systems/Movement/GroundMoveMode.h"
#include "Core/Systems/Movement/FlyMoveMode.h"

#include "Physics/IPhysicsWorld.h"
#include "Physics/Collision/CollisionProfile.h"
#include "Physics/Collision/CollisionChannel.h"

#include "World/World.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Components/TransformInterpolationComponent.h"

namespace Opaax
{
    bool MoverSubsystem::Startup()
    {
        // Built-in modes. Game/engine code adds more via MoverModeRegistry::Register.
        MoverModeRegistry::Register(OPAAX_ID("GroundMove"), MakeUnique<GroundMoveMode>());
        MoverModeRegistry::Register(OPAAX_ID("Fly"),        MakeUnique<FlyMoveMode>());

        OPAAX_CORE_INFO("MoverSubsystem::Startup — {} mode(s) registered.",
                        MoverModeRegistry::GetModeIds().size());
        return true;
    }

    void MoverSubsystem::Shutdown()
    {
        // World may already be gone at engine shutdown — ClearBodies null-guards.
        if (CoreEngineApp* lApp = GetEngineApp())
        {
            PhysicsSubsystem* lPhysics = lApp->GetSubsystem<PhysicsSubsystem>();
            ClearBodies(lPhysics ? lPhysics->GetWorld() : nullptr);
        }
        MoverModeRegistry::Clear();
        OPAAX_CORE_INFO("MoverSubsystem::Shutdown");
    }

    // =============================================================================
    // Play-session edges — the mover's kinematic bodies churn per play session
    // =============================================================================
    void MoverSubsystem::OnPlayBegin()
    {
        CoreEngineApp* lApp = GetEngineApp();
        if (lApp == nullptr) { return; }

        PhysicsSubsystem* lPhysics = lApp->GetSubsystem<PhysicsSubsystem>();
        IPhysicsWorld*    lWorld   = lPhysics ? lPhysics->GetWorld() : nullptr;
        if (lWorld == nullptr) { return; }

        // Reconcile per-mode params — covers code-created movers that never went through AddMode/load.
        lApp->GetWorld().Each<ECS::MoverComponent>(
            [](EntityID /*InEntity*/, ECS::MoverComponent& InMover) { InMover.EnsureModeParams(); });

        BuildBodies(lApp->GetWorld(), *lWorld);
        OPAAX_CORE_INFO("MoverSubsystem::OnPlayBegin — built {} mover bodies.", m_Bodies.size());
    }

    void MoverSubsystem::OnPlayEnd()
    {
        const size_t lCount = m_Bodies.size();
        if (CoreEngineApp* lApp = GetEngineApp())
        {
            PhysicsSubsystem* lPhysics = lApp->GetSubsystem<PhysicsSubsystem>();
            ClearBodies(lPhysics ? lPhysics->GetWorld() : nullptr);
        }
        OPAAX_CORE_INFO("MoverSubsystem::OnPlayEnd — destroyed {} mover bodies.", lCount);
    }

    // =============================================================================
    // Body build / teardown
    // =============================================================================
    void MoverSubsystem::BuildBodies(World& InWorld, IPhysicsWorld& InPhysicsWorld)
    {
        ClearBodies(&InPhysicsWorld);

        using namespace ECS;
        InWorld.Each<MoverComponent, TransformComponent>(
            [this, &InWorld, &InPhysicsWorld](EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform)
            {
                // A mover is a KINEMATIC body — we drive it; the solver never moves it. Fixed rotation.
                BodyDesc lBodyDesc;
                lBodyDesc.Type          = EBodyType::Kinematic;
                lBodyDesc.Position      = InTransform.Position;
                lBodyDesc.Rotation      = InTransform.Rotation;
                lBodyDesc.FixedRotation = true;
                lBodyDesc.UserData      = static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1;

                const BodyHandle lBody = InPhysicsWorld.CreateBody(lBodyDesc);
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

                InPhysicsWorld.AddShape(lBody, lShapeDesc);
                m_Bodies.emplace(static_cast<Uint32>(InEntity), lBody);

                // Seed render-interpolation prev pose to the start pose (frame 0 must not pop).
                InWorld.AddOrReplaceComponent<TransformInterpolationComponent>(
                    InEntity, TransformInterpolationComponent{ InTransform.Position, InTransform.Rotation });
            });
    }

    void MoverSubsystem::ClearBodies(IPhysicsWorld* InPhysicsWorld)
    {
        if (InPhysicsWorld != nullptr)
        {
            for (auto& [lBits, lBody] : m_Bodies)
            {
                InPhysicsWorld->DestroyBody(lBody);
            }
        }
        m_Bodies.clear();
    }

    void MoverSubsystem::FixedUpdate(double FixedDeltaTime)
    {
        CoreEngineApp* lApp = GetEngineApp();
        if (lApp == nullptr) { return; }

        // Reuse the physics world (no second world). Null when not playing -> nothing to move.
        PhysicsSubsystem* lPhysics = lApp->GetSubsystem<PhysicsSubsystem>();
        IPhysicsWorld*    lWorld   = lPhysics ? lPhysics->GetWorld() : nullptr;
        if (lWorld == nullptr) { return; }

        const float lDt = static_cast<float>(FixedDeltaTime);

        using namespace ECS;
        World& lEcsWorld = lApp->GetWorld();
        lEcsWorld.Each<MoverComponent, TransformComponent>(
            [this, lWorld, lDt, &lEcsWorld](EntityID InEntity, MoverComponent& InMover, TransformComponent& InTransform)
            {
                const Uint32 lBits = static_cast<Uint32>(InEntity);

                // Snapshot the pre-tick pose for render interpolation before the mode mutates
                // the transform — prev/current then bracket exactly one fixed step.
                if (auto* lInterp = lEcsWorld.GetComponent<TransformInterpolationComponent>(InEntity))
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
                        // A mover can only switch to a mode it was authored with (Modes set).
                        if (m_WarnedUnknownModes.insert(lNext.GetId()).second)
                        {
                            OPAAX_CORE_WARN("MoverSubsystem — mover does not support mode '{}'; switch ignored.", lNext);
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
                            OPAAX_CORE_WARN("MoverSubsystem — mover has no supported modes; entity skipped.");
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
                        OPAAX_CORE_WARN("MoverSubsystem — no mover mode registered for '{}'; entity skipped.",
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
                        OPAAX_CORE_WARN("MoverSubsystem — no params for mode '{}'; entity skipped.", InMover.ModeId);
                    }
                    return;
                }

                lMode->Tick(lContext);

                // Drive the kinematic body to the mode's solved pose: it moves there during the next
                // physics Step — colliding, pushing dynamics, and firing contact/sensor events (which
                // PhysicsSubsystem resolves back to this entity by user-data). No extra event code.
                const auto lBodyIt = m_Bodies.find(lBits);
                if (lBodyIt != m_Bodies.end())
                {
                    lWorld->SetBodyTargetTransform(lBodyIt->second, InTransform.Position,
                                                   InTransform.Rotation, lDt);
                }
            });
    }

} // namespace Opaax
