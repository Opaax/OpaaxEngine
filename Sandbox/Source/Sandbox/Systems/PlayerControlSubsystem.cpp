#include "Systems/PlayerControlSubsystem.h"

#include "Engine/Subsystems/Input/InputCodes.h"
#include "Engine/Subsystems/Input/InputManager.h"
#include "World/Components/MoverComponent.h"
#include "World/Components/TransformComponent.h"
#include "World/Systems/WorldContext.h"
#include "World/World.h"

using namespace Opaax;

namespace Sandbox
{
    namespace
    {
        constexpr LogCategory LogPlayerControl{"PlayerControl"};

        /** The two modes the Hero mover names. Switching is Tab. */
        const OpaaxStringID kGroundMode = OPAAX_ID("Ground");
        const OpaaxStringID kFlyMode    = OPAAX_ID("Fly");
    }

    bool PlayerControlSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool PlayerControlSubsystem::Startup()
    {
        OPAAX_LOG(LogPlayerControl, Info,
                  "Player control started — A/D or arrows to move, Space to jump, Tab to switch mode");
        return true;
    }

    void PlayerControlSubsystem::Shutdown()
    {
        OPAAX_LOG(LogPlayerControl, Info, "Player control shutdown ({} mover(s) driven)", m_LastDriven);
    }

    void PlayerControlSubsystem::Update(double)
    {
        const InputManager& lInput = m_Context->Input;

        // Read ONCE per frame, then written to every mover — the intent is what the human asked
        // for, not something each entity re-derives.
        const bool bLeft  = lInput.IsKeyDown(EKeyCode::A) || lInput.IsKeyDown(EKeyCode::Left);
        const bool bRight = lInput.IsKeyDown(EKeyCode::D) || lInput.IsKeyDown(EKeyCode::Right);
        const bool bUp    = lInput.IsKeyDown(EKeyCode::W) || lInput.IsKeyDown(EKeyCode::Up);
        const bool bDown  = lInput.IsKeyDown(EKeyCode::S) || lInput.IsKeyDown(EKeyCode::Down);

        // An EDGE, not a held state: WasPressedThisFrame is latched by the feed, so a tap that
        // starts and ends inside one frame still counts. The mode consumes it when it is spent.
        const bool bJump   = lInput.WasPressedThisFrame(EKeyCode::Space);
        const bool bSwitch = lInput.WasPressedThisFrame(EKeyCode::Tab);

        Vector2F lMoveDir{ 0.f, 0.f };
        if (bLeft)  { lMoveDir.x -= 1.f; }
        if (bRight) { lMoveDir.x += 1.f; }
        if (bDown)  { lMoveDir.y -= 1.f; }
        if (bUp)    { lMoveDir.y += 1.f; }

        Uint64 lDriven = 0;

        m_Context->OwningWorld.Each<MoverComponent, TransformComponent>(
            [&](EntityID, MoverComponent& InMover, TransformComponent&)
            {
                InMover.Input.MoveDir = lMoveDir;

                // OR rather than assign: the edge is cleared by the MODE when it spends it, so
                // overwriting with false here would eat a jump the mover had not used yet.
                if (bJump) { InMover.Input.bJump = true; }

                if (bSwitch)
                {
                    // Queued, not applied — the subsystem fires OnModeExit/OnModeEnter between
                    // steps rather than mid-step.
                    InMover.PendingMode = (InMover.ModeName == kFlyMode) ? kGroundMode : kFlyMode;
                }

                ++lDriven;
            });

        m_LastDriven = lDriven;

        if (!m_bLoggedFirstTick)
        {
            m_bLoggedFirstTick = true;
            OPAAX_LOG(LogPlayerControl, Info, "Driving {} mover(s)", lDriven);
        }
    }
}
