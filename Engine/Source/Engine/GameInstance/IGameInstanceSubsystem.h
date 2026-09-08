#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IGameInstanceSubsystem — marker interface for game-session-owned subsystems.
    //   Lifetime = one game (StartGame -> EndGame), which outlives any single World.
    // =============================================================================
    class OPAAX_API IGameInstanceSubsystem : public ISubsystem
    {
    };

    class OPAAX_API GameInstanceSubsystemBase : public IGameInstanceSubsystem
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        GameInstanceSubsystemBase()           = default;
        ~GameInstanceSubsystemBase() override = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================
        GameInstanceSubsystemBase(const GameInstanceSubsystemBase&)            = delete;
        GameInstanceSubsystemBase& operator=(const GameInstanceSubsystemBase&) = delete;
        GameInstanceSubsystemBase(GameInstanceSubsystemBase&&)                 = delete;
        GameInstanceSubsystemBase& operator=(GameInstanceSubsystemBase&&)      = delete;
    };

    // =============================================================================
    // GameInstanceSubsystemMgr — owns + drives the game-instance subsystem list.
    // =============================================================================
    class OPAAX_API GameInstanceSubsystemMgr : public ISubsystemManager<IGameInstanceSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~GameInstanceSubsystemMgr() override = default;
    };
}
