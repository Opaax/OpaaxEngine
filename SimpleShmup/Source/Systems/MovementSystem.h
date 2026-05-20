#pragma once

#include "Core/Systems/GameSubsystem.h"

/**
 * @class MovementSystem
 *
 * Integrates VelocityComponent into TransformComponent::Position every tick.
 * Registered as a game subsystem by SimpleShmupGame::OnInitialize; PIE-gated
 * automatically (IsPlayOnly defaults to true on IGameSubsystem).
 */
class MovementSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(MovementSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    MovementSystem() = default;
    explicit MovementSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem interface
public:
    void Update(double DeltaTime) override;
    //~End IGameSubsystem interface
};
