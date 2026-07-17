#pragma once

#include "Core/Systems/GameSubsystem.h"

class CollisionSystem;

// =============================================================================
// ShmupGameRulesSystem
//
// Reads CollisionSystem::GetHits() each tick and applies shmup-specific
// reactions: Bullet vs Enemy destroys both and scores; Player vs Enemy is
// detected and ignored (G2 follow-up). All other pair combinations are
// ignored. The CollisionSystem pointer is cached lazily on first Update —
// registration order makes the manager lookup safe by that point.
// =============================================================================
class ShmupGameRulesSystem final : public Opaax::GameSubsystemBase
{
    // =========================================================================
    // Identification
    // =========================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(ShmupGameRulesSystem)

    // =========================================================================
    // CTORs
    // =========================================================================
public:
    ShmupGameRulesSystem() = default;
    explicit ShmupGameRulesSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =========================================================================
    // Override
    // =========================================================================
    //~Begin IGameSubsystem interface
public:
    void Update(double DeltaTime) override;
    //~End IGameSubsystem interface

    // =========================================================================
    // Public config
    // =========================================================================
public:
    Opaax::Uint32 ScorePerKill = 100;

    // =========================================================================
    // Members
    // =========================================================================
private:
    CollisionSystem* m_Collision = nullptr;
};
