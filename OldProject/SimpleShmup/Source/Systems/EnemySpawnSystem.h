#pragma once

#include <random>

#include "Core/Systems/GameSubsystem.h"
#include "Core/OpaaxMathTypes.h"
#include "Core/OpaaxTypes.h"

/**
 * @class EnemySpawnSystem
 *
 * Timer-driven enemy spawner. Every SpawnInterval seconds, if the alive enemy
 * count is below MaxAlive, spawns one enemy at the right edge with a random Y
 * in SpawnYRange and a leftward velocity. Lifetime is a safety net for any
 * enemy that escapes the playfield before collision claims it.
 */
class EnemySpawnSystem final : public Opaax::GameSubsystemBase
{
    // =============================================================================
    // Identification
    // =============================================================================
public:
    OPAAX_SUBSYSTEM_TYPE(EnemySpawnSystem)

    // =============================================================================
    // CTORs
    // =============================================================================
public:
    EnemySpawnSystem();
    explicit EnemySpawnSystem(Opaax::CoreEngineApp* InEngineApp);

    // =============================================================================
    // Override
    // =============================================================================
    //~Begin IGameSubsystem interface
public:
    void Update(double DeltaTime) override;
    //~End IGameSubsystem interface

    // =============================================================================
    // Public config (tweakable from game-app OnInitialize)
    // =============================================================================
public:
    float           SpawnInterval = 1.5f;
    Opaax::Uint32   MaxAlive      = 8;
    float           EnemySpeed    = 200.f;
    Opaax::Vector2F SpawnYRange   = { -300.f, 300.f };
    float           SpawnX        = 640.f;
    float           EnemyLifetime = 8.f;

    // =============================================================================
    // Members
    // =============================================================================
private:
    float        m_SpawnTimer = 0.f;
    std::mt19937 m_Rng;
};
