#include "World/Systems/MoverSubsystem.h"

#include "Application/OpaaxApplication.h"
#include "Application/Services/IEngine.h"
#include "Application/Services/IPaths.h"
#include "Core/Maths/Maths.h"
#include "Core/Profiling/FrameProfiler.h"
#include "Engine/Registries/EngineRegistries.h"
#include "Engine/Subsystems/Resources/ResourceManager.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoveModeResource.h"
#include "Engine/Subsystems/Resources/Types/Mover/MoverResource.h"
#include "Physics/IPhysicsWorld.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Components/TransformInterpolationComponent.h"
#include "World/Systems/Movement/MoverModeRegistry.h"
#include "World/Systems/PhysicsSubsystem.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

namespace Opaax
{
    namespace
    {
        /** Entity bits + 1, matching PhysicsSubsystem's encoding so the sweep can skip its own body. */
        Uint64 ToUserData(EntityID InEntity) noexcept
        {
            return static_cast<Uint64>(static_cast<Uint32>(InEntity)) + 1ull;
        }
    }

    // =========================================================================
    // Base implementation
    // =========================================================================
    bool MoverSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    // =========================================================================
    // Lifecycle
    // =========================================================================
    bool MoverSubsystem::Startup()
    {
        // Resolved from the manager, never through a lazy accessor that could re-enter boot (F3/L6).
        IEngine& lEngine = OpaaxApplication::GetAppService<IEngine>();

        m_Modes = &lEngine.GetRegistries().MoverModes();

        OPAAX_LOG(LogMover, Info, "Mover started (Play world, {} mode(s) registered)", m_Modes->Count());
        return true;
    }

    void MoverSubsystem::Shutdown()
    {
        m_MoverCache.clear();
        m_ModeCache.clear();

        OPAAX_LOG(LogMover, Info, "Mover shutdown ({} mover(s) on the last step)", m_LastAdvanced);
    }

    // =========================================================================
    // Tick
    // =========================================================================
    void MoverSubsystem::FixedUpdate(const double InFixedDeltaTime)
    {
        OPAAX_STAT_SCOPE(m_Context->Profiler, "Mover");

        World& lWorld = m_Context->OwningWorld;

        // A mover sweeps against the physics world, so without one there is nothing to sweep
        // against — not an error, just a world where nothing moves this way.
        PhysicsSubsystem* lPhysics = lWorld.GetSubsystems().GetSubsystem<PhysicsSubsystem>();
        IPhysicsWorld*    lSweep   = lPhysics != nullptr ? lPhysics->GetPhysicsWorld() : nullptr;

        if (lSweep == nullptr || m_Modes == nullptr)
        {
            m_LastAdvanced = 0;
            return;
        }

        const float lDelta   = static_cast<float>(InFixedDeltaTime);
        Uint64      lAdvanced = 0;

        lWorld.Each<MoverComponent, TransformComponent>(
            [this, lSweep, lDelta, &lAdvanced](EntityID InEntity, MoverComponent& InMover,
                                               TransformComponent& InTransform)
            {
                Advance(InEntity, InMover, InTransform, *lSweep, lDelta);
                ++lAdvanced;
            });

        m_LastAdvanced = lAdvanced;

        if (!m_bLoggedFirstStep)
        {
            m_bLoggedFirstStep = true;
            OPAAX_LOG(LogMover, Info, "Advancing {} mover(s)", lAdvanced);
        }
    }

    // =========================================================================
    // One entity
    // =========================================================================
    void MoverSubsystem::Advance(const EntityID InEntity, MoverComponent& InMover,
                                 TransformComponent& InTransform, IPhysicsWorld& InWorld,
                                 const float InDelta)
    {
        // ---- the pending switch, applied BETWEEN steps rather than inside one -----------------
        if (InMover.PendingMode.IsValid() && InMover.PendingMode != InMover.ModeName)
        {
            const MoveModeData* lFromParams = ResolveMode(InMover, InMover.ModeName);
            const MoveModeData* lToParams   = ResolveMode(InMover, InMover.PendingMode);

            // A switch to a mode that cannot resolve is REFUSED rather than half-applied — leaving
            // a mover naming a mode with no tuning would stop it dead with nothing to say.
            if (lToParams == nullptr)
            {
                InMover.PendingMode = OpaaxStringID();
            }
            else
            {
                if (lFromParams != nullptr)
                {
                    if (IMoverMode* lFrom = m_Modes->Find(lFromParams->Mode))
                    {
                        MoverTickContext lExit{ InWorld, InMover, InTransform, *lFromParams, 0.f, 0 };
                        lFrom->OnModeExit(lExit);
                    }
                }

                InMover.ModeName    = InMover.PendingMode;
                InMover.PendingMode = OpaaxStringID();

                if (IMoverMode* lTo = m_Modes->Find(lToParams->Mode))
                {
                    MoverTickContext lEnter{ InWorld, InMover, InTransform, *lToParams, 0.f, 0 };
                    lTo->OnModeEnter(lEnter);
                }

                OPAAX_LOG(LogMover, Info, "Entity {} switched to mode '{}' ({})",
                          static_cast<Uint32>(InEntity), InMover.ModeName.CStr(), lToParams->Mode.CStr());
            }
        }

        const MoveModeData* lParams = ResolveMode(InMover, InMover.ModeName);

        if (lParams == nullptr)
        {
            return;   // ResolveMode warned once about whichever half was missing
        }

        IMoverMode* lMode = m_Modes->Find(lParams->Mode);

        if (lMode == nullptr)
        {
            if (ShouldWarnOnce(lParams->Mode.GetId()))
            {
                OPAAX_LOG(LogMover, Warn, "No mover mode named '{}' is registered — nothing moves it",
                          lParams->Mode.CStr());
            }
            return;
        }

        // Before the mode writes: the pose it is about to overwrite is what the renderer blends
        // FROM (**PH21**). Physics records its own the same way, one subsystem over.
        auto& lPrevious = m_Context->OwningWorld.GetRegistry()
                                    .get_or_emplace<TransformInterpolationComponent>(InEntity);
        lPrevious.Position     = InTransform.Position;
        lPrevious.Rotation     = InTransform.Rotation;
        lPrevious.bHasPrevious = true;

        MoverTickContext lTick{ InWorld, InMover, InTransform, *lParams, InDelta, ToUserData(InEntity) };
        lMode->Tick(lTick);

        if (m_ProbeEntity == ENTITY_NONE)
        {
            m_ProbeEntity = InEntity;
            m_ProbeOrigin = InTransform.Position;
        }

        NoteMoverMoved(InEntity, InTransform.Position);
    }

    // =========================================================================
    // Resolution
    // =========================================================================
    const MoveModeData* MoverSubsystem::ResolveMode(const MoverComponent& InMover,
                                                    const OpaaxStringID InModeName)
    {
        if (InMover.Mover.IsEmpty())
        {
            return nullptr;   // naming no mover is a real state, not a failure worth a line
        }

        // ---- the bag ---------------------------------------------------------------------
        const OpaaxStringID lMoverKey(InMover.Mover.Path);

        auto lMoverIt = m_MoverCache.find(lMoverKey.GetId());

        if (lMoverIt == m_MoverCache.end())
        {
            ResourceRef<MoverResource> lRef =
                m_Context->Resources.Load<MoverResource>(ToAbsolute(InMover.Mover.Path).CStr());

            lMoverIt = m_MoverCache.emplace(lMoverKey.GetId(), Move(lRef)).first;

            if (!lMoverIt->second.IsValid())
            {
                OPAAX_LOG(LogMover, Warn, "Mover '{}' did not load — no mode name can resolve",
                          InMover.Mover.Path.CStr());
            }
        }

        const MoverResource* lMoverRes = lMoverIt->second.Get();

        if (lMoverRes == nullptr)
        {
            return nullptr;
        }

        // ---- the entry the name resolves to ------------------------------------------------
        const MoverEntry* lEntry = lMoverRes->Data.Find(InModeName);

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

        // ---- the tuning ---------------------------------------------------------------------
        const OpaaxStringID lModeKey(lEntry->ModeAsset.Path);

        auto lModeIt = m_ModeCache.find(lModeKey.GetId());

        if (lModeIt == m_ModeCache.end())
        {
            ResourceRef<MoveModeResource> lRef =
                m_Context->Resources.Load<MoveModeResource>(ToAbsolute(lEntry->ModeAsset.Path).CStr());

            lModeIt = m_ModeCache.emplace(lModeKey.GetId(), Move(lRef)).first;

            if (!lModeIt->second.IsValid())
            {
                OPAAX_LOG(LogMover, Warn, "Move mode '{}' did not load", lEntry->ModeAsset.Path.CStr());
            }
        }

        const MoveModeResource* lModeRes = lModeIt->second.Get();

        return (lModeRes != nullptr) ? &lModeRes->Data : nullptr;
    }

    OpaaxString MoverSubsystem::ToAbsolute(const OpaaxString& InAssetPath) const
    {
        return m_Context->Paths.AssetToAbsolute(InAssetPath);
    }

    bool MoverSubsystem::ShouldWarnOnce(const Uint32 InKey)
    {
        return m_Warned.emplace(InKey).second;
    }

    void MoverSubsystem::NoteMoverMoved(const EntityID InEntity, const Vector2F& InPosition)
    {
        if (m_bLoggedMotion || InEntity != m_ProbeEntity)
        {
            return;
        }

        // "Advancing N mover(s)" is a COUNT, and it prints the same for N movers standing still —
        // the [[L76]] trap. A whole world unit, so float noise cannot pass for motion ([[L21]]).
        const Vector2F lDelta    = InPosition - m_ProbeOrigin;
        const float    lDistance = Maths::Sqrt(lDelta.x * lDelta.x + lDelta.y * lDelta.y);

        if (lDistance < 1.f)
        {
            return;
        }

        m_bLoggedMotion = true;
        OPAAX_LOG(LogMover, Info, "Mover {} has moved {:.1f} units — the mode is driving it",
                  static_cast<Uint32>(InEntity), lDistance);
    }
}
