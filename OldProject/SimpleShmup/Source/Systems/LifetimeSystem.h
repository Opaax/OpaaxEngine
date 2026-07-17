#pragma once

#include "Core/Systems/GameSubsystem.h"

/**
 * @class LifetimeSystem
 *
 * Decrements LifetimeComponent::SecondsRemaining every tick and destroys
 * entities whose countdown has expired. Two-pass: collect doomed IDs during
 * iteration, then DestroyEntity post-iteration (Lesson 12 — never destroy
 * during view iteration). PIE-gated by default.
 */
class LifetimeSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(LifetimeSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    LifetimeSystem() = default;
    explicit LifetimeSystem(Opaax::CoreEngineApp* InEngineApp) : GameSubsystemBase(InEngineApp) {}

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem interface
public:
    void Update(double DeltaTime) override;
    //~End IGameSubsystem interface
};
