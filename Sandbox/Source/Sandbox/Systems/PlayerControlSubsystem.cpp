#include "Systems/PlayerControlSubsystem.h"

#include "Engine/Input/InputActionValue.h"
#include "Engine/Input/InputMappingSubsystem.h"
#include "Engine/Input/InputTypes.h"
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

        /** The two modes the Hero mover names. */
        const OpaaxStringID kGroundMode = OPAAX_ID("Ground");
        const OpaaxStringID kFlyMode    = OPAAX_ID("Fly");

        /** The actions this game binds. The KEYS live in the mapping asset, not here. */
        const OpaaxStringID kMoveAction   = OPAAX_ID("Move");
        const OpaaxStringID kJumpAction   = OPAAX_ID("Jump");
        const OpaaxStringID kSwitchAction = OPAAX_ID("SwitchMode");

        /** The context this game plays under. */
        const OpaaxStringID kGameplayContext = OPAAX_ID("Gameplay");

        constexpr const char* kGameplayMapAsset = "Input/Gameplay.opaaxinputmap";
    }

    bool PlayerControlSubsystem::ShouldCreate(const World& InWorld)
    {
        return InWorld.GetMode() == EWorldMode::Play;
    }

    bool PlayerControlSubsystem::Startup()
    {
        InputMappingSubsystem* lActions = m_Context->Actions;

        if (lActions == nullptr)
        {
            // Play-only, so a game must exist — if it does not, say so rather than tick silently
            // doing nothing for the rest of the session.
            OPAAX_LOG(LogPlayerControl, Error,
                      "Player control started with NO input mapping — this world has no game instance, so nothing will drive the movers.");
            return true;
        }

        lActions->AddContextAsset(kGameplayContext, kGameplayMapAsset);

        lActions->Bind(kMoveAction,   EInputTrigger::Triggered, this, &PlayerControlSubsystem::OnMove);
        lActions->Bind(kMoveAction,   EInputTrigger::Completed, this, &PlayerControlSubsystem::OnMoveCompleted);
        lActions->Bind(kJumpAction,   EInputTrigger::Started,   this, &PlayerControlSubsystem::OnJump);
        lActions->Bind(kSwitchAction, EInputTrigger::Started,   this, &PlayerControlSubsystem::OnSwitchMode);

        OPAAX_LOG(LogPlayerControl, Info,
                  "Player control started — {} action(s) over {} context(s), {} binding(s)",
                  lActions->GetActionCount(), lActions->GetContextCount(), lActions->GetBindingCount());
        return true;
    }

    void PlayerControlSubsystem::Shutdown()
    {
        if (m_Context->Actions != nullptr)
        {
            const Uint64 lRemoved = m_Context->Actions->UnbindAll(this);

            OPAAX_LOG(LogPlayerControl, Info, "Player control shutdown ({} mover(s) driven, {} binding(s) removed)",
                      m_LastDriven, lRemoved);
            return;
        }

        OPAAX_LOG(LogPlayerControl, Info, "Player control shutdown ({} mover(s) driven)", m_LastDriven);
    }

    // =========================================================================
    // Action handlers — run BEFORE Update, from the GameInstance's tick
    // =========================================================================
    bool PlayerControlSubsystem::IsOwningWorldActive() const
    {
        return m_Context->OwningWorld.IsActive();
    }

    void PlayerControlSubsystem::OnMove(const InputActionValue& InValue)
    {
        if (!IsOwningWorldActive()) { return; }

        m_MoveDir = InValue.AsAxis2D();
    }

    void PlayerControlSubsystem::OnMoveCompleted(const InputActionValue& /*InValue*/)
    {
        if (!IsOwningWorldActive()) { return; }

        // Triggered stops firing when the value reaches zero, so without this the last direction
        // would stick and the mover would walk into the wall forever.
        m_MoveDir = Vector2F{0.f, 0.f};
    }

    void PlayerControlSubsystem::OnJump(const InputActionValue& /*InValue*/)
    {
        if (!IsOwningWorldActive()) { return; }

        m_bJumpQueued = true;
    }

    void PlayerControlSubsystem::OnSwitchMode(const InputActionValue& /*InValue*/)
    {
        if (!IsOwningWorldActive()) { return; }

        m_bSwitchQueued = true;
    }

    // =========================================================================
    // Tick
    // =========================================================================
    void PlayerControlSubsystem::Update(double)
    {
        Uint64 lDriven = 0;

        m_Context->OwningWorld.Each<MoverComponent, TransformComponent>(
            [&](EntityID, MoverComponent& InMover, TransformComponent&)
            {
                InMover.Input.MoveDir = m_MoveDir;

                // OR rather than assign: the edge is cleared by the MODE when it spends it, so
                // overwriting with false here would eat a jump the mover had not used yet.
                if (m_bJumpQueued) { InMover.Input.bJump = true; }

                if (m_bSwitchQueued)
                {
                    // Queued, not applied — the subsystem fires OnModeExit/OnModeEnter between
                    // steps rather than mid-step.
                    InMover.PendingMode = (InMover.ModeName == kFlyMode) ? kGroundMode : kFlyMode;
                }

                ++lDriven;
            });

        // Spent HERE, after every mover has seen them: the latches exist so a press that lands
        // between two ticks still reaches the world exactly once.
        m_bJumpQueued   = false;
        m_bSwitchQueued = false;

        m_LastDriven = lDriven;

        if (!m_bLoggedFirstTick)
        {
            m_bLoggedFirstTick = true;
            OPAAX_LOG(LogPlayerControl, Info, "Driving {} mover(s)", lDriven);
        }
    }
}
