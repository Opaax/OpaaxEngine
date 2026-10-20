#pragma once

#include "Engine/Subsystems/Subsystem.h"

namespace Opaax
{
    struct GameInstanceContext;

    // =============================================================================
    // GameInstanceSubsystem — a subsystem that lives for one game (StartGame -> EndGame), across
    //   level changes. Every game makes one of each registered concrete class, in GetTickOrder() order.
    // =============================================================================
    class GameInstanceSubsystem : public Subsystem
    {
        OPAAX_ABSTRACT_OBJECT(GameInstanceSubsystem, Subsystem)

    public:
        /** The game's services. Valid from Startup to Shutdown. */
        GameInstanceContext& GetContext() const;

    private:
        friend class GameInstanceSubsystemMgr;

        GameInstanceContext* m_Context = nullptr;   // owned by the GameInstance
    };

    // =============================================================================
    // GameInstanceSubsystemMgr — owns and ticks one game's subsystems, and hands each the game's
    //   context before its Startup.
    // =============================================================================
    class GameInstanceSubsystemMgr final : public TSubsystemCollection<GameInstanceSubsystem>
    {
    public:
        /** The context every subsystem gets. Set by the GameInstance before StartupAll. */
        void SetContext(GameInstanceContext* InContext) noexcept { m_Context = InContext; }

    protected:
        void OnCreated(GameInstanceSubsystem& InSubsystem) override { InSubsystem.m_Context = m_Context; }

    private:
        GameInstanceContext* m_Context = nullptr;   // owned by the GameInstance
    };
}
