#include "Movement/MoverSubsystem.h"

#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/Profiling/Profiler.h"
#include "Resources/ResourceManager.h"
#include "Movement/Assets/MoveMode.h"
#include "Movement/Assets/Mover.h"
#include "Movement/Modes/GroundMoveMode.h"
#include "Physics/IPhysicsWorld.h"
#include "Movement/MoverComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Entity/EntityHierarchy.h"   // modes move the world pose
#include "Physics/PhysicsSubsystem.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** Entity bits + 1, like PhysicsSubsystem, so the sweep can skip its own body. */
        Uint64 ToUserData(EntityID InEntity) noexcept
        {
            return static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1ull;
        }
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool MoverSubsystem::ShouldCreate(const World& InWorld) const
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    void MoverSubsystem::Shutdown()
    {
        m_MoverCache.clear();
        m_ModeCache.clear();
    }

    // =========================================================================
    // Tick
    // =========================================================================
    void MoverSubsystem::FixedUpdate(const double InFixedDeltaTime)
    {
        OPAAX_STAT_SCOPE("Mover");

        World& lWorld = GetContext().OwningWorld;

        // No physics world: nothing to sweep against, nothing moves. Not an error.
        PhysicsSubsystem* lPhysics = lWorld.GetSubsystems().GetSubsystem<PhysicsSubsystem>();
        IPhysicsWorld*    lSweep   = lPhysics != nullptr ? lPhysics->GetPhysicsWorld() : nullptr;

        if (lSweep == nullptr)
        {
            m_LastAdvanced = 0;
            return;
        }

        const float lDelta   = static_cast<float>(InFixedDeltaTime);
        Uint64      lAdvanced = 0;

        lWorld.Each<MoverComponent, TransformComponent>(
            [this, &lWorld, lSweep, lDelta, &lAdvanced](EntityID InEntity, MoverComponent& InMover,
                                                        TransformComponent& InLocal)
            {
                // Record the local pose before the mode changes it (for render interpolation).
                auto& lPrevious = lWorld.GetRegistry().get_or_emplace<TransformInterpolationComponent>(InEntity);
                lPrevious.Position     = InLocal.Position;
                lPrevious.Rotation     = InLocal.Rotation;
                lPrevious.bHasPrevious = true;

                // Modes work in world space; the result is stored back as local.
                const Entity       lLive{ InEntity, &lWorld };
                TransformComponent lWorldXf = EntityHierarchy::WorldTransform(lLive);

                Advance(InEntity, InMover, lWorldXf, *lSweep, lDelta);

                EntityHierarchy::SetWorldTransform(lLive, lWorldXf);
                ++lAdvanced;
            });

        m_LastAdvanced = lAdvanced;
    }

    // =========================================================================
    // One entity
    // =========================================================================
    void MoverSubsystem::Advance(const EntityID InEntity, MoverComponent& InMover,
                                 TransformComponent& InTransform, IPhysicsWorld& InWorld,
                                 const float InDelta)
    {
        const Entity lOwner{ InEntity, &GetContext().OwningWorld };

        // ---- pending mode switch, applied between steps ---------------------------------------
        if (InMover.PendingMode.IsValid() && InMover.PendingMode != InMover.ModeName)
        {
            const MoveMode* lFrom = ResolveMode(InMover, InMover.ModeName);
            const MoveMode* lTo   = ResolveMode(InMover, InMover.PendingMode);

            // Refuse a switch to a mode that cannot be resolved.
            if (lTo == nullptr)
            {
                InMover.PendingMode = OpaaxStringID();
            }
            else
            {
                if (lFrom != nullptr)
                {
                    MoverTickContext lExit{ InWorld, InMover, InTransform, 0.f, 0, lOwner };
                    lFrom->OnModeExit(lExit);
                }

                InMover.ModeName    = InMover.PendingMode;
                InMover.PendingMode = OpaaxStringID();

                MoverTickContext lEnter{ InWorld, InMover, InTransform, 0.f, 0, lOwner };
                lTo->OnModeEnter(lEnter);

                OPAAX_LOG(LogMover, Info, "Entity {} switched to mode '{}' ({})",
                          static_cast<Uint32>(InEntity), InMover.ModeName.CStr(), lTo->GetClass().GetName().CStr());
            }
        }

        const MoveMode* lMode = ResolveMode(InMover, InMover.ModeName);

        if (lMode == nullptr)
        {
            return;   // ResolveMode already warned
        }

        MoverTickContext lTick{ InWorld, InMover, InTransform, InDelta, ToUserData(InEntity), lOwner };
        lMode->Tick(lTick);
    }

    // =========================================================================
    // Resolution
    // =========================================================================
    const MoveMode* MoverSubsystem::ResolveMode(const MoverComponent& InMover,
                                                const OpaaxStringID InModeName)
    {
        if (InMover.Mover.IsEmpty())
        {
            return nullptr;   // no mover set: not an error
        }

        // ---- the bag ---------------------------------------------------------------------
        const OpaaxStringID lMoverKey(InMover.Mover.Path);

        auto lMoverIt = m_MoverCache.find(lMoverKey.GetId());

        if (lMoverIt == m_MoverCache.end())
        {
            lMoverIt = m_MoverCache.emplace(lMoverKey.GetId(),
                                            LoadAsset(GetContext().Resources, GetContext().Paths, InMover.Mover)).first;

            if (lMoverIt->second.Get() == nullptr)
            {
                OPAAX_LOG(LogMover, Warn, "Mover '{}' did not load — no mode name can resolve",
                          InMover.Mover.Path.CStr());
            }
        }

        const Mover* lMover = lMoverIt->second.Get();

        if (lMover == nullptr)
        {
            return nullptr;
        }

        // ---- the entry the name resolves to ------------------------------------------------
        const MoverEntry* lEntry = lMover->Find(InModeName);

        if (lEntry == nullptr)
        {
            if (ShouldWarnOnce(InModeName.GetId()))
            {
                OPAAX_LOG(LogMover, Warn, "Mover '{}' has no mode named '{}'",
                          InMover.Mover.Path.CStr(),
                          InModeName.IsValid() ? InModeName.CStr() : "(default)");
            }
            return nullptr;
        }

        if (lEntry->ModeAsset.IsEmpty())
        {
            return nullptr;
        }

        // ---- the mode -----------------------------------------------------------------------
        const OpaaxStringID lModeKey(lEntry->ModeAsset.Path);

        auto lModeIt = m_ModeCache.find(lModeKey.GetId());

        if (lModeIt == m_ModeCache.end())
        {
            lModeIt = m_ModeCache.emplace(lModeKey.GetId(),
                                          LoadAsset(GetContext().Resources, GetContext().Paths, lEntry->ModeAsset)).first;

            if (lModeIt->second.Get() == nullptr)
            {
                OPAAX_LOG(LogMover, Warn, "Move mode '{}' did not load — it walks with the default tuning",
                          lEntry->ModeAsset.Path.CStr());
            }
        }

        // A mode that does not load still moves: it walks, with the defaults.
        if (const MoveMode* lMode = lModeIt->second.Get())
        {
            return lMode;
        }

        return Cast<MoveMode>(GroundMoveMode::StaticClass().GetDefaultObject());
    }

    bool MoverSubsystem::ShouldWarnOnce(const Uint32 InKey)
    {
        return m_Warned.emplace(InKey).second;
    }
}
