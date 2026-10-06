#pragma once

#include "Core/Systems/Subsystem.h"

namespace Opaax
{
    // =============================================================================
    // IGameInstanceSubsystem — a subsystem that lives for one game (StartGame -> EndGame),
    //   across level changes.
    // =============================================================================
    class IGameInstanceSubsystem : public ISubsystem
    {
    };

    class GameInstanceSubsystemBase : public IGameInstanceSubsystem
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
    // GameInstanceSubsystemMgr — owns and ticks the game-instance subsystems.
    // =============================================================================
    class GameInstanceSubsystemMgr : public ISubsystemManager<IGameInstanceSubsystem>
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        ~GameInstanceSubsystemMgr() override = default;
    };
}
