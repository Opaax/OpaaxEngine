#include "MoverSubsystem.h"

#include "Core/CoreEngineApp.h"
#include "Core/Log/OpaaxLog.h"

#include "Core/Systems/PhysicsSubsystem.h"
#include "Core/Systems/Movement/MoverModeRegistry.h"
#include "Core/Systems/Movement/IMoverMode.h"
#include "Core/Systems/Movement/GroundMoveMode.h"
#include "Core/Systems/Movement/FlyMoveMode.h"

#include "World/World.h"
#include "ECS/Components/MoverComponent.h"
#include "ECS/Components/TransformComponent.h"

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
        MoverModeRegistry::Clear();
        OPAAX_CORE_INFO("MoverSubsystem::Shutdown");
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
        lApp->GetWorld().Each<MoverComponent, TransformComponent>(
            [this, lWorld, lDt](EntityID /*InEntity*/, MoverComponent& InMover, TransformComponent& InTransform)
            {
                MoverTickContext lContext{ *lWorld, InMover, InTransform, lDt };

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

                lMode->Tick(lContext);
            });
    }

} // namespace Opaax
